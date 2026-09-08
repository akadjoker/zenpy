/*
** compiler.cpp — Infrastructure: compile(), advance, consume, errors,
** variable/name resolution, register management, scope management.
**
** Split:
**   compiler.cpp              — this file (infrastructure + top-level compile)
**   compiler_expressions.cpp  — Pratt expression parser
**   compiler_statements.cpp   — statement/declaration parsing
*/

#include "compiler.h"

namespace zen
{

    /* =========================================================
    ** Top-level: compile source → ObjFunc*
    ** ========================================================= */

    ObjFunc *Compiler::compile(GC *gc, VM *vm, const char *source, const char *filename)
    {
        expr_depth_ = 0;
        gc_ = gc;
        vm_ = vm;

        /* Pause GC during compilation — intermediate objects (strings,
           functions, constants) are not yet rooted and would be swept
           by a stress-mode GC cycle. */
        gc_pause(gc_);

        had_error_ = false;
        panic_mode_ = false;
        silent_ = false;
        error_count_ = 0;
        abort_parse_ = false;
        err_buf_[0] = '\0';
        err_buf_used_ = 0;
        err_count_ = 0;
        current_file_ = filename;
        in_class_ = false;
        class_has_parent_ = false;
        pending_decorator_count_ = 0;
        class_field_count_ = 0;
        class_field_default_count_ = 0;
        class_registry_count_ = 0;
        global_type_hint_count_ = 0;
        pending_callee_valid_ = false;
        pending_receiver_valid_ = false;
        pending_subscript_receiver_valid_ = false;
        typed_subscript_reg_ = -1;
        typed_call_reg_ = -1;
        last_expr_ctor_valid_ = false;
        multi_assign_rhs_ = false;
        cmp_chain_end_ = -1;
        fn_written_global_count_ = 0;
        last_cmp_.valid = false;

        sigs_ = nullptr; sig_count_ = 0; sig_cap_ = 0;
        sig_params_ = nullptr; sig_param_count_ = 0; sig_param_cap_ = 0;
        sig_classes_ = nullptr; sig_class_count_ = 0; sig_class_cap_ = 0;
        prescan_signatures(source, filename);

        lexer_.init(source, filename);

        /* Set up the script-level compiler state */
        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc_);
        script_state.emitter = Emitter(gc_);
        script_state.local_count = 0;
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;
        script_state.eval_mode = false;
        script_state.had_eval_return = false;
        script_state.is_generator = false;
        script_state.global_count = 0;

        script_state.emitter.begin("<module>", 0, filename);
        state_ = &script_state;

        /* Prime the parser */
        advance();

        /* Parse top-level declarations until EOF */
        while (!check(TOK_EOF))
        {
            if (abort_parse_)
                break;
            /* Skip stray newlines at top level */
            if (match(TOK_NEWLINE))
                continue;
            declaration();
        }

        /* Implicit return nil at end of script */
        state_->emitter.emit_abc(OP_RETURNNIL, 0, 1, 0, previous_.line);

        ObjFunc *script = state_->emitter.end(state_->max_reg);
        script->arity = 0;

        free_signatures();
        gc_resume(gc_);
        return had_error_ ? nullptr : script;
    }

    ObjFunc *Compiler::compile_eval(GC *gc, VM *vm, const char *source, const char *filename, bool silent)
    {
        gc_ = gc;
        vm_ = vm;
        gc_pause(gc_);
        had_error_ = false;
        panic_mode_ = false;
        silent_ = silent;
        error_count_ = 0;
        abort_parse_ = false;
        err_buf_[0] = '\0';
        err_buf_used_ = 0;
        err_count_ = 0;
        current_file_ = filename;
        in_class_ = false;
        class_has_parent_ = false;
        pending_decorator_count_ = 0;
        pending_callee_valid_ = false;
        pending_receiver_valid_ = false;
        pending_subscript_receiver_valid_ = false;
        typed_subscript_reg_ = -1;
        typed_call_reg_ = -1;
        last_expr_ctor_valid_ = false;
        multi_assign_rhs_ = false;
        cmp_chain_end_ = -1;
        fn_written_global_count_ = 0;
        last_cmp_.valid = false;
        global_type_hint_count_ = 0;

        sigs_ = nullptr; sig_count_ = 0; sig_cap_ = 0;
        sig_params_ = nullptr; sig_param_count_ = 0; sig_param_cap_ = 0;
        sig_classes_ = nullptr; sig_class_count_ = 0; sig_class_cap_ = 0;
        prescan_signatures(source, filename);

        lexer_.init(source, filename);

        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc_);
        script_state.emitter = Emitter(gc_);
        script_state.local_count = 0;
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;
        script_state.eval_mode = true;
        script_state.had_eval_return = false;
        script_state.is_generator = false;
        script_state.global_count = 0;

        script_state.emitter.begin("<eval>", 0, filename);
        state_ = &script_state;

        advance();

        /* EVAL_MODE: parse a single expression, put result in R[0], return it */
        /* Skip leading newlines */
        while (match(TOK_NEWLINE)) {}

        if (!check(TOK_EOF))
        {
            /* Reserve R[0] as the result register so sub-expressions don't stomp on it */
            state_->next_reg = 1;
            expression(0); /* result in R[0] */

            /* Consume any trailing newline then expect EOF */
            while (match(TOK_NEWLINE)) {}
            if (!check(TOK_EOF))
            {
                /* Leftover tokens — this isn't a pure expression (e.g. "x = 1") */
                had_error_ = true;
            }
            else
            {
                state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
            }
        }
        else
        {
            state_->emitter.emit_abc(OP_RETURNNIL, 0, 1, 0, previous_.line);
        }

        ObjFunc *script = state_->emitter.end(state_->max_reg);
        script->arity = 0;

        free_signatures();
        gc_resume(gc_);
        return had_error_ ? nullptr : script;
    }

    /* =========================================================
    ** Lexer interface
    ** ========================================================= */

    void Compiler::advance()
    {
        previous_ = current_;
        for (;;)
        {
            current_ = lexer_.next_token();
            if (current_.type != TOK_ERROR)
                break;
            error_at_current(current_.start);
        }
    }

    void Compiler::consume(TokenType type, const char *msg)
    {
        if (current_.type == type)
        {
            advance();
            return;
        }
        error_at_current(msg);
    }

    bool Compiler::check(TokenType type) const
    {
        return current_.type == type;
    }

    bool Compiler::match(TokenType type)
    {
        if (!check(type))
            return false;
        advance();
        return true;
    }

    /* =========================================================
    ** Error reporting
    ** ========================================================= */

    void Compiler::error(const char *msg)
    {
        error_at(previous_, msg);
    }

    void Compiler::error_at_current(const char *msg)
    {
        error_at(current_, msg);
    }

    void Compiler::error_at(const Token &token, const char *msg)
    {
        if (panic_mode_ || abort_parse_)
            return;
        panic_mode_ = true;
        had_error_ = true;
        error_count_++;

        /* Capture into POD buffer */
        if (err_count_ < kMaxErrorInfos) {
            err_info_[err_count_].line = token.line;
            err_info_[err_count_].offset = err_buf_used_;
            int avail = kErrorBufSize - err_buf_used_;
            if (avail > 1) {
                int n = snprintf(err_buf_ + err_buf_used_, avail, "%s", msg);
                if (n > 0 && n < avail)
                    err_buf_used_ += n + 1; /* include null terminator */
            }
            err_count_++;
        }

        if (!silent_)
        {
            fprintf(stderr, "[line %d] Error", token.line);
            if (token.type == TOK_EOF)
                fprintf(stderr, " at end");
            else if (token.type != TOK_ERROR)
                fprintf(stderr, " at '%.*s'", token.length, token.start);
            fprintf(stderr, ": %s\n", msg);

            if (error_count_ >= kMaxCompileErrors)
            {
                fprintf(stderr, "Too many compile errors (%d). Aborting parse.\n", error_count_);
            }
        }

        /* Fail-fast: stop immediately on first compile error. */
        abort_parse_ = true;
    }

    /* =========================================================
    ** Register allocation
    **
    ** Simple bump allocator. Registers are freed in reverse order.
    ** max_reg tracks the high-water mark for the frame size.
    ** ========================================================= */

    int Compiler::alloc_reg()
    {
        int reg = state_->next_reg++;
        if (reg == typed_subscript_reg_)
            typed_subscript_reg_ = -1;
        if (reg == typed_call_reg_)
            typed_call_reg_ = -1;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;
        if (reg >= kMaxRegisters)
        {
            error("Too many registers needed (expression too complex).");
            return 0;
        }
        return reg;
    }

    void Compiler::free_reg(int reg)
    {
        /* Only free if it's the top register AND not a local */
        if (reg == state_->next_reg - 1)
        {
            /* Check it's not a declared local */
            for (int i = 0; i < state_->local_count; i++)
            {
                if (state_->locals[i].reg == reg)
                    return; /* don't free locals */
            }
            state_->next_reg--;
            if (reg == typed_subscript_reg_)
                typed_subscript_reg_ = -1;
        }
    }

    void Compiler::emit_move(int dst, int src)
    {
        if (dst != src)
            state_->emitter.emit_abc(OP_MOVE, dst, src, 0, previous_.line);
    }

    bool Compiler::is_local_reg(int reg) const
    {
        for (int i = 0; i < state_->local_count; i++)
            if (state_->locals[i].reg == reg)
                return true;
        return false;
    }

    bool Compiler::retarget_last_producer(int rhs_start, int jumps_before, int src, int dst)
    {
        if (src == dst)
            return true;
        Emitter &e = state_->emitter;
        int off = e.current_offset() - 1;
        if (off < rhs_start)
            return false; /* the RHS emitted nothing (a bare local) */
        if (e.last_op_start() != off)
            return false; /* last instruction is multi-word: its tail is data */
        if (e.jump_count() != jumps_before)
            return false; /* control flow inside the RHS */
        if (is_local_reg(src))
            return false; /* the value lives in a variable, not a temporary */
        Instruction ins = e.instruction_at(off);
        if (ZEN_A(ins) != src)
            return false;
        switch (ZEN_OP(ins))
        {
        /* Every one of these reads its operands, then stores R[A] once. */
        case OP_MOVE: case OP_LOADK: case OP_LOADI:
        case OP_GETGLOBAL: case OP_GETUPVAL:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_IDIV: case OP_MOD: case OP_POW:
        case OP_ADDI: case OP_SUBI: case OP_NEG:
        case OP_BAND: case OP_BOR: case OP_BXOR: case OP_BNOT: case OP_SHL: case OP_SHR:
        case OP_EQ: case OP_LT: case OP_LE: case OP_NOT:
        case OP_GETFIELD_IDX: case OP_GETINDEX: case OP_CONCAT: case OP_LEN:
            break;
        default:
            return false;
        }
        e.patch_a_at(off, dst);
        last_cmp_.valid = false; /* its boolean no longer lives in `src` */
        if (typed_subscript_reg_ == src)
            typed_subscript_reg_ = -1;
        if (typed_call_reg_ == src)
            typed_call_reg_ = -1;
        return true;
    }

    int Compiler::cond_false_jump(int reg, bool &fused)
    {
        Emitter &e = state_->emitter;
        int off = e.current_offset() - 1;
        /* A pending literal comparison (`x != 0`, `x == None`) is emitted
        ** as EQ+NOT too; emit_cond_jump() has the better (immediate) form. */
        const bool literal_cmp_pending = last_cmp_.valid && last_cmp_.end_offset == e.current_offset() &&
                                         last_cmp_.reg == reg;
        if (off >= 0 && e.last_op_start() == off && !is_local_reg(reg) && !literal_cmp_pending &&
            cmp_chain_end_ != e.current_offset())
        {
            Instruction ins = e.instruction_at(off);
            if (ZEN_OP(ins) == OP_NOT && ZEN_A(ins) == reg)
            {
                /* `if not xs[i]:` — the operand was computed into its own
                ** temporary just above `reg` (or is reg itself) and
                ** nothing ran since. */
                int operand = ZEN_B(ins);
                int line = e.line_at(off);
                e.shrink_to(off);
                last_cmp_.valid = false;
                /* `a != b` is EQ + NOT with both on the same register — that
                ** shape only comes from comparison(), which emits the EQ as
                ** the single word right before (so off-1 is a real head
                ** word, not the data word of something longer). Fuse. */
                int off2 = off - 1;
                if (off2 >= 0 && operand == reg && !is_local_reg(operand))
                {
                    Instruction cmp = e.instruction_at(off2);
                    if (ZEN_OP(cmp) == OP_EQ && ZEN_A(cmp) == operand)
                    {
                        int b = ZEN_B(cmp), c = ZEN_C(cmp);
                        int cline = e.line_at(off2);
                        e.shrink_to(off2);
                        fused = true;
                        free_reg(reg);
                        return e.emit_cmpi_jmpifnot(OP_NEJMPIFNOT, b, c, cline);
                    }
                }
                fused = false;
                free_reg(reg);
                return e.emit_jump(OP_JMPIF, operand, line);
            }
        }
        int j = emit_cond_jump(reg, fused);
        free_reg(reg);
        return j;
    }

    int Compiler::condition(CondJump *jumps, int &n)
    {
        n = 0;
        int reg = parse_precedence(PREC_AND + 1, -1);
        if (had_error_)
            return reg;
        if (!check(TOK_AND) && !check(TOK_OR))
        {
            /* Plain condition. A ternary is the only operator that binds
            ** looser than `and`/`or` and can still follow. */
            if (check(TOK_IF) && current_.line == previous_.line)
            {
                advance();
                reg = infix_rule(previous_, reg, -1);
            }
            return reg;
        }

        int true_jumps[kMaxCondJumps];
        int n_true = 0;
        while (true)
        {
            if (n >= kMaxCondJumps || n_true >= kMaxCondJumps)
            {
                error("Condition has too many and/or operands.");
                return -1;
            }
            bool fused = false;
            jumps[n].offset = cond_false_jump(reg, fused);
            jumps[n].fused = fused;
            n++;
            if (match(TOK_AND))
            {
                reg = parse_precedence(PREC_AND + 1, -1);
                if (had_error_)
                    return -1;
                continue;
            }
            if (match(TOK_OR))
            {
                /* The and-chain so far held: straight to the body. Its
                ** false jumps land on the next operand instead. */
                true_jumps[n_true++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
                patch_cond_jumps(jumps, n);
                n = 0;
                reg = parse_precedence(PREC_AND + 1, -1);
                if (had_error_)
                    return -1;
                continue;
            }
            break;
        }
        if (check(TOK_IF) && current_.line == previous_.line)
            error("A conditional expression cannot follow an and/or chain in a condition; parenthesize it.");
        for (int i = 0; i < n_true; i++)
            state_->emitter.patch_jump(true_jumps[i]);
        return -1;
    }

    void Compiler::patch_cond_jumps(const CondJump *jumps, int n)
    {
        for (int i = 0; i < n; i++)
            patch_cond_jump(jumps[i].offset, jumps[i].fused);
    }

    int Compiler::emit_cond_jump(int cond, bool &fused)
    {
        Emitter &e = state_->emitter;
        fused = false;

        /* `x < 2`, `x >= 0`, `x == None`, `x is None` (and negations) as a
        ** branch condition: drop the literal load and the boolean, branch
        ** on the operand and the literal directly. */
        if (last_cmp_.valid && last_cmp_.end_offset == e.current_offset() &&
            last_cmp_.reg == cond && !is_local_reg(cond) && !is_local_reg(last_cmp_.rhs) &&
            last_cmp_.lhs != last_cmp_.rhs && last_cmp_.lhs != cond)
        {
            LastCmp c = last_cmp_;
            last_cmp_.valid = false;
            int line = e.line_at(c.load_offset);
            if (c.imm_kind == 1)
            {
                OpCode op = OP_LTIJMPIFNOT;
                bool ok = true;
                switch (c.op)
                {
                case TOK_LT:   op = OP_LTIJMPIFNOT; break;
                case TOK_LTEQ: op = OP_LEIJMPIFNOT; break;
                case TOK_GT:   op = OP_GTIJMPIFNOT; break;
                case TOK_GTEQ: op = OP_GEIJMPIFNOT; break;
                case TOK_EQEQ: op = OP_EQIJMPIFNOT; break;
                case TOK_BANGEQ: op = OP_NEIJMPIFNOT; break;
                default: ok = false; break;
                }
                if (ok)
                {
                    e.shrink_to(c.load_offset);
                    fused = true;
                    return e.emit_cmpi_jmpifnot(op, c.lhs, c.imm, line);
                }
            }
            else if (c.imm_kind == 2)
            {
                OpCode op = OP_JMPIFNEQNIL;
                bool ok = true;
                switch (c.op)
                {
                case TOK_EQEQ:   op = OP_JMPIFNEQNIL; break; /* `if x == None:` skips when x is not None */
                case TOK_BANGEQ: op = OP_JMPIFEQNIL;  break;
                case TOK_IS:     op = c.negated ? OP_JMPIFNIL : OP_JMPIFNOTNIL; break;
                default: ok = false; break;
                }
                if (ok)
                {
                    e.shrink_to(c.load_offset);
                    return e.emit_jump(op, c.lhs, line);
                }
            }
        }
        last_cmp_.valid = false;

        int off = e.current_offset() - 1;
        if (off >= 0 && e.last_op_start() == off)
        {
            Instruction cmp = e.instruction_at(off);
            OpCode cop = (OpCode)ZEN_OP(cmp);
            if (ZEN_A(cmp) == cond && (cop == OP_LT || cop == OP_LE || cop == OP_EQ) &&
                !is_local_reg(cond) && cmp_chain_end_ != e.current_offset())
            {
                /* The comparison's boolean was only ever going to be
                ** branched on: drop it for the fused compare-and-jump,
                ** whose handler keeps string and overloaded-operator
                ** semantics. */
                int b = ZEN_B(cmp), c = ZEN_C(cmp);
                int line = e.line_at(off);
                e.shrink_to(off);
                fused = true;
                if (cop == OP_EQ)
                    return e.emit_cmpi_jmpifnot(OP_EQJMPIFNOT, b, c, line);
                return cop == OP_LE ? e.emit_le_jmpifnot(b, c, line) : e.emit_lt_jmpifnot(b, c, line);
            }
        }
        return e.emit_jump(OP_JMPIFNOT, cond, previous_.line);
    }

    void Compiler::patch_cond_jump(int offset, bool fused)
    {
        if (fused)
            state_->emitter.patch_fused_jump(offset);
        else
            state_->emitter.patch_jump(offset);
    }

    /* =========================================================
    ** Scope management
    ** ========================================================= */

    void Compiler::begin_scope()
    {
        state_->scope_depth++;
    }

    void Compiler::end_scope()
    {
        state_->scope_depth--;

        /* Close locals that go out of scope */
        while (state_->local_count > 0 &&
               state_->locals[state_->local_count - 1].depth > state_->scope_depth)
        {
            Local &local = state_->locals[state_->local_count - 1];
            if (local.captured)
            {
                state_->emitter.emit_abc(OP_CLOSE, local.reg, 0, 0, previous_.line);
            }
            state_->local_count--;
            /* Free the register */
            if (local.reg == state_->next_reg - 1)
                state_->next_reg--;
        }
    }

    /* Emit OP_CLOSE for the lowest captured local register (before return/move) */
    void Compiler::close_captured_locals()
    {
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].captured)
            {
                state_->emitter.emit_abc(OP_CLOSE, state_->locals[i].reg, 0, 0, previous_.line);
                return; /* one OP_CLOSE at lowest reg closes all >= that reg */
            }
        }
    }

    /* =========================================================
    ** Local variable management
    ** ========================================================= */

    int Compiler::declare_local(const Token &name)
    {
        /* Check for redeclaration in same scope */
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            Local &local = state_->locals[i];
            if (local.depth < state_->scope_depth)
                break;
            if (identifiers_equal(local.name, name))
            {
                error("Variable already declared in this scope.");
                return -1;
            }
        }
        return add_local(name);
    }

    int Compiler::add_local(const Token &name)
    {
        if (state_->local_count >= kMaxLocals)
        {
            error("Too many local variables.");
            return -1;
        }
        int reg = alloc_reg();
        Local &local = state_->locals[state_->local_count++];
        local.name = name;
        local.depth = state_->scope_depth;
        local.reg = reg;
        local.captured = false;
        local.is_const = false;
        local.has_type_hint = false;
        local.has_array_element_type = false;
        local.type_inferred = false;
        local.type_exact = false;
        return reg;
    }

    /* =========================================================
    ** Class field table — compile-time O(1) field index lookup
    ** ========================================================= */

    int Compiler::lookup_class_field(ObjString *name) const
    {
        for (int i = 0; i < class_field_count_; i++)
            if (class_field_table_[i] == name) return i;
        return -1;
    }

    int Compiler::add_class_field(ObjString *name)
    {
        int idx = lookup_class_field(name);
        if (idx >= 0) return idx;
        if (class_field_count_ >= kMaxClassFields) return -1;
        class_field_table_[class_field_count_] = name;
        class_field_class_state_[class_field_count_] = 0;
        return class_field_count_++;
    }

    bool Compiler::class_field_literal(int &out_const_index)
    {
        bool negate = false;
        if (match(TOK_MINUS))
            negate = true;

        if (match(TOK_INT))
        {
            int64_t v = strtoll(previous_.start, nullptr, 0);
            out_const_index = state_->emitter.add_constant(val_int(negate ? -v : v));
            return true;
        }
        if (match(TOK_FLOAT))
        {
            double v = strtod(previous_.start, nullptr);
            out_const_index = state_->emitter.add_constant(val_float(negate ? -v : v));
            return true;
        }
        if (negate)
            return false; /* "-" followed by something that is not a number */

        if (match(TOK_STRING))
        {
            /* Same quote handling as string_literal(): single or triple. */
            const char *str = previous_.start + 1;
            int len = previous_.length - 2;
            if (previous_.length >= 6 && previous_.start[0] == previous_.start[1] &&
                previous_.start[1] == previous_.start[2])
            {
                str = previous_.start + 3;
                len = previous_.length - 6;
            }
            out_const_index = state_->emitter.add_escaped_string_constant(str, len);
            if (state_->emitter.has_escape_error())
            {
                error(state_->emitter.escape_error());
                state_->emitter.clear_escape_error();
            }
            return true;
        }
        if (match(TOK_TRUE))
        {
            out_const_index = state_->emitter.add_constant(val_bool(true));
            return true;
        }
        if (match(TOK_FALSE))
        {
            out_const_index = state_->emitter.add_constant(val_bool(false));
            return true;
        }
        if (match(TOK_NONE))
        {
            out_const_index = state_->emitter.add_constant(val_nil());
            return true;
        }
        return false;
    }

    /* Returns true when `reg` holds a known instance of the current class.
    ** Always true for self (reg 0 in any method). */
    bool Compiler::is_current_class_instance(int reg) const
    {
        /* Only `self` earns the unchecked direct index: it is an instance
        ** of this class (or a subclass, whose layout extends it) by
        ** construction. An annotated local or parameter of this class is
        ** a promise, not a fact — it takes the checked form instead. */
        if (!state_->is_method || !in_class_) return false;
        return reg == 0;
    }

    /* Save current class field table to the registry. Called after class_declaration(). */
    void Compiler::save_class_fields(Token class_name)
    {
        if (class_registry_count_ >= kMaxClasses) return;
        ClassFieldRegistry &reg = class_registry_[class_registry_count_++];
        reg.name = class_name;
        reg.count = class_field_count_;
        for (int i = 0; i < class_field_count_; i++)
        {
            reg.fields[i] = class_field_table_[i];
            reg.field_class[i] = class_field_class_[i];
            reg.field_class_state[i] = class_field_class_state_[i];
        }
    }

    /* Look up a previously-compiled class's fields and initialise the
    ** class_field_table_ from them.  Returns true if parent was found. */
    bool Compiler::inherit_class_fields(Token parent_name)
    {
        for (int i = 0; i < class_registry_count_; i++)
        {
            if (identifiers_equal(class_registry_[i].name, parent_name))
            {
                int cnt = class_registry_[i].count;
                class_field_count_ = cnt;
                for (int j = 0; j < cnt; j++)
                {
                    class_field_table_[j] = class_registry_[i].fields[j];
                    class_field_class_[j] = class_registry_[i].field_class[j];
                    class_field_class_state_[j] = class_registry_[i].field_class_state[j];
                }
                return true;
            }
        }

        /* Fallback: parent may be a C++ class defined via ClassBuilder.
        ** Look it up in the VM globals and copy its field_names. */
        if (vm_)
        {
            char buf[256];
            int len = parent_name.length < (int)sizeof(buf) - 1
                          ? parent_name.length
                          : (int)sizeof(buf) - 1;
            memcpy(buf, parent_name.start, len);
            buf[len] = '\0';
            int idx = vm_->find_global(buf);
            if (idx >= 0 && is_class(vm_->get_global(idx)))
            {
                ObjClass *pcls = as_class(vm_->get_global(idx));
                int cnt = pcls->num_fields;
                class_field_count_ = cnt;
                for (int j = 0; j < cnt; j++)
                {
                    class_field_table_[j] = pcls->field_names[j];
                    class_field_class_state_[j] = 0;
                }
                return true;
            }
        }

        return false;
    }

    /* =========================================================
    ** Name resolution — the heart of compile-time work.
    **
    ** Searches: locals → upvalues → globals
    ** Returns the register (or emits load instruction).
    ** ========================================================= */

    int Compiler::resolve_local(CompilerState *state, const Token &name)
    {
        for (int i = state->local_count - 1; i >= 0; i--)
        {
            if (identifiers_equal(state->locals[i].name, name))
                return state->locals[i].reg;
        }
        return -1;
    }

    int Compiler::resolve_upvalue(CompilerState *state, const Token &name)
    {
        if (!state->parent)
            return -1;

        /* Check parent's locals */
        int local = resolve_local(state->parent, name);
        if (local != -1)
        {
            /* resolve_local() returns the REGISTER; find the local that
            ** owns it (indices and registers diverge once temporaries sit
            ** between locals). A closure may rebind it at any time, so a
            ** class inferred from an assignment is no longer a fact. */
            for (int i = state->parent->local_count - 1; i >= 0; i--)
            {
                Local &cap = state->parent->locals[i];
                if (cap.reg != local)
                    continue;
                cap.captured = true;
                if (cap.type_inferred)
                {
                    cap.has_type_hint = false;
                    cap.type_inferred = false;
                    cap.type_exact = false;
                }
                break;
            }
            return add_upvalue(state, local, true);
        }

        /* Check parent's upvalues (recursive) */
        int upvalue = resolve_upvalue(state->parent, name);
        if (upvalue != -1)
            return add_upvalue(state, upvalue, false);

        return -1;
    }

    int Compiler::add_upvalue(CompilerState *state, int index, bool is_local)
    {
        /* Check if already captured */
        for (int i = 0; i < state->upvalue_count; i++)
        {
            if (state->upvalues[i].index == index && state->upvalues[i].is_local == is_local)
                return i;
        }
        if (state->upvalue_count >= kMaxUpvalues)
        {
            error("Too many upvalues in function.");
            return 0;
        }
        state->upvalues[state->upvalue_count].index = index;
        state->upvalues[state->upvalue_count].is_local = is_local;
        return state->upvalue_count++;
    }

    int Compiler::named_variable(const Token &name, int dest, bool can_assign)
    {
        int reg = resolve_local(state_, name);

        if (reg != -1)
        {
            /* Local variable */
            if (can_assign && match(TOK_EQ))
            {
                /* Check const */
                for (int i = state_->local_count - 1; i >= 0; i--)
                {
                    Local &l = state_->locals[i];
                    if (l.reg == reg && l.is_const)
                    {
                        error("Cannot assign to const variable.");
                        return reg;
                    }
                }
                int val = expression(reg);
                if (val != reg)
                    emit_move(reg, val);
                infer_assigned_class_local(reg);
                return reg;
            }
            /* Augmented assignment */
            if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                              check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                              check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                              check(TOK_DSTAR_EQ) || check(TOK_AMP_EQ) || check(TOK_PIPE_EQ) || check(TOK_CARET_EQ) || check(TOK_LSHIFT_EQ) || check(TOK_RSHIFT_EQ)))
            {
                /* Check const */
                for (int i = state_->local_count - 1; i >= 0; i--)
                {
                    Local &l = state_->locals[i];
                    if (l.reg == reg && l.is_const)
                    {
                        error("Cannot assign to const variable.");
                        return reg;
                    }
                }
                Token op = current_;
                advance();
                last_expr_ctor_valid_ = false;
                infer_assigned_class_local(reg); /* drops an inferred class */
                int rhs_start = state_->emitter.current_offset();
                int rhs = expression(-1);

                /* The ordinary binary-expression peephole already turns
                ** `x = x + 1` into ADDI. Apply the identical encoding to
                ** the idiomatic `x += 1` form: a loop counter should not
                ** pay a LOADI temporary merely because it uses augmented
                ** assignment. OP_ADDI/SUBI retain the full ADD/SUB contract
                ** (numbers, strings and overloaded instances). */
                if ((op.type == TOK_PLUS_EQ || op.type == TOK_MINUS_EQ) &&
                    state_->emitter.current_offset() == rhs_start + 1)
                {
                    Instruction li = state_->emitter.instruction_at(rhs_start);
                    int imm = ZEN_SBX(li);
                    if (ZEN_OP(li) == OP_LOADI && ZEN_A(li) == rhs &&
                        imm >= -128 && imm <= 127)
                    {
                        state_->emitter.shrink_to(rhs_start);
                        free_reg(rhs);
                        state_->emitter.emit_abc(op.type == TOK_PLUS_EQ ? OP_ADDI : OP_SUBI,
                                                 reg, reg, (uint8_t)(int8_t)imm, op.line);
                        return reg;
                    }
                }
                OpCode arith = OP_ADD;
                switch (op.type)
                {
                case TOK_PLUS_EQ:    arith = OP_ADD;  break;
                case TOK_MINUS_EQ:   arith = OP_SUB;  break;
                case TOK_STAR_EQ:    arith = OP_MUL;  break;
                case TOK_SLASH_EQ:   arith = OP_DIV;  break;
                case TOK_PERCENT_EQ: arith = OP_MOD;  break;
                case TOK_DSLASH_EQ:  arith = OP_IDIV; break;
                case TOK_DSTAR_EQ:   arith = OP_POW;  break;   case TOK_AMP_EQ:   arith = OP_BAND;   break;   case TOK_PIPE_EQ:   arith = OP_BOR;   break;   case TOK_CARET_EQ:   arith = OP_BXOR;   break;   case TOK_LSHIFT_EQ:   arith = OP_SHL;   break;   case TOK_RSHIFT_EQ:   arith = OP_SHR;   break;
                default: break;
                }
                state_->emitter.emit_abc(arith, reg, reg, rhs, previous_.line);
                free_reg(rhs);
                return reg;
            }
            /* Read */
            if (dest >= 0 && dest != reg)
            {
                /* `n = self.item`, `v = xs[i]`, `r = f(x)`: the link that
                ** follows reads the local where it lives and puts its own
                ** result in dest — copying the local into dest first only
                ** hid the receiver from the typed/self paths (a MOVE plus a
                ** by-name GETFIELD instead of one GETFIELD_IDX). */
                if (chain_continues())
                    return reg;
                emit_move(dest, reg);
                return dest;
            }
            return reg;
        }

        /* Upvalue */
        int upval = resolve_upvalue(state_, name);
        if (upval != -1)
        {
            int r = (dest >= 0) ? dest : alloc_reg();
            if (can_assign && match(TOK_EQ))
            {
                int val = expression(r);
                if (val != r) emit_move(r, val);
                state_->emitter.emit_abc(OP_SETUPVAL, r, upval, 0, previous_.line);
                return r;
            }
            state_->emitter.emit_abc(OP_GETUPVAL, r, upval, 0, previous_.line);
            return r;
        }

        /* Intrinsic: len(x) → OP_LEN */
        if (name.length == 3 && memcmp(name.start, "len", 3) == 0 && check(TOK_LPAREN))
        {
            advance(); /* consume '(' */
            int r = (dest >= 0) ? dest : alloc_reg();
            int arg = expression(-1);
            consume(TOK_RPAREN, "Expected ')' after argument to len().");
            state_->emitter.emit_abc(OP_LEN, r, arg, 0, previous_.line);
            free_reg(arg);
            state_->next_reg = r + 1;
            if (state_->next_reg > state_->max_reg) state_->max_reg = state_->next_reg;
            return r;
        }

        /* Global */
        int gidx = find_or_add_global(name.start, name.length);
        int r = (dest >= 0) ? dest : alloc_reg();

        if (can_assign && match(TOK_EQ))
        {
            /* `g = g + x` is the same operation as `g += x`, and the += path
            ** already emits the non-marking AUG pair so ADD can append a
            ** string in place. Written the long way it took OP_GETGLOBAL,
            ** which marks the string shared, so every append copied the whole
            ** accumulator: 200k appends went from 0.01s to 51s, quadratic.
            **
            ** Detect it after the fact: the RHS is one instruction, that
            ** instruction reads the global we are assigning, and it was
            ** loaded by the GETGLOBAL immediately before. Rewrite the pair
            ** rather than re-parse. */
            int rhs_start = state_->emitter.current_offset();
            int val = expression(r);
            if (val != r) emit_move(r, val);

            bool aug = false;
            int off = state_->emitter.current_offset() - 1;
            if (off >= rhs_start + 1)
            {
                Instruction last = state_->emitter.instruction_at(off);
                OpCode lop = (OpCode)ZEN_OP(last);
                /* Only the ops whose AUG form is meaningful — a string
                ** accumulator is ADD; the rest are here because += accepts
                ** them and the shape is identical. */
                if ((lop == OP_ADD || lop == OP_SUB || lop == OP_MUL ||
                     lop == OP_DIV || lop == OP_MOD || lop == OP_IDIV ||
                     lop == OP_POW || lop == OP_BAND || lop == OP_BOR ||
                     lop == OP_BXOR || lop == OP_SHL || lop == OP_SHR) &&
                    ZEN_A(last) == r)
                {
                    Instruction first = state_->emitter.instruction_at(rhs_start);
                    if (ZEN_OP(first) == OP_GETGLOBAL &&
                        (int)ZEN_BX(first) == gidx &&
                        ZEN_B(last) == (int)ZEN_A(first))
                    {
                        state_->emitter.rewrite_opcode_at(rhs_start, OP_GETGLOBAL_AUG);
                        aug = true;
                    }
                }
            }

            state_->emitter.emit_abx(aug ? OP_SETGLOBAL_AUG : OP_SETGLOBAL,
                                     r, gidx, previous_.line);
            note_global_written_in_function(gidx);
            infer_assigned_class_global(gidx);
            return r;
        }
        /* Augmented assignment on global */
        if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                           check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                           check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                           check(TOK_DSTAR_EQ) || check(TOK_AMP_EQ) || check(TOK_PIPE_EQ) || check(TOK_CARET_EQ) || check(TOK_LSHIFT_EQ) || check(TOK_RSHIFT_EQ)))
        {
            Token op = current_;
            advance();
            note_global_written_in_function(gidx);
            last_expr_ctor_valid_ = false;
            infer_assigned_class_global(gidx); /* drops an inferred class */
            /* Non-marking pair: keeps a global string accumulator unshared
            ** so ADD can append in place — see OP_GETGLOBAL_AUG. */
            state_->emitter.emit_abx(OP_GETGLOBAL_AUG, r, gidx, previous_.line);
            int rhs_start = state_->emitter.current_offset();
            int rhs = expression(-1);

            if ((op.type == TOK_PLUS_EQ || op.type == TOK_MINUS_EQ) &&
                state_->emitter.current_offset() == rhs_start + 1)
            {
                Instruction li = state_->emitter.instruction_at(rhs_start);
                int imm = ZEN_SBX(li);
                if (ZEN_OP(li) == OP_LOADI && ZEN_A(li) == rhs &&
                    imm >= -128 && imm <= 127)
                {
                    state_->emitter.shrink_to(rhs_start);
                    free_reg(rhs);
                    state_->emitter.emit_abc(op.type == TOK_PLUS_EQ ? OP_ADDI : OP_SUBI,
                                             r, r, (uint8_t)(int8_t)imm, op.line);
                    state_->emitter.emit_abx(OP_SETGLOBAL_AUG, r, gidx, previous_.line);
                    return r;
                }
            }
            OpCode arith = OP_ADD;
            switch (op.type)
            {
            case TOK_PLUS_EQ:    arith = OP_ADD;  break;
            case TOK_MINUS_EQ:   arith = OP_SUB;  break;
            case TOK_STAR_EQ:    arith = OP_MUL;  break;
            case TOK_SLASH_EQ:   arith = OP_DIV;  break;
            case TOK_PERCENT_EQ: arith = OP_MOD;  break;
            case TOK_DSLASH_EQ:  arith = OP_IDIV; break;
            case TOK_DSTAR_EQ:   arith = OP_POW;  break;   case TOK_AMP_EQ:   arith = OP_BAND;   break;   case TOK_PIPE_EQ:   arith = OP_BOR;   break;   case TOK_CARET_EQ:   arith = OP_BXOR;   break;   case TOK_LSHIFT_EQ:   arith = OP_SHL;   break;   case TOK_RSHIFT_EQ:   arith = OP_SHR;   break;
            default: break;
            }
            state_->emitter.emit_abc(arith, r, r, rhs, previous_.line);
            free_reg(rhs);
            state_->emitter.emit_abx(OP_SETGLOBAL_AUG, r, gidx, previous_.line);
            return r;
        }

        /* Read global */
        state_->emitter.emit_abx(OP_GETGLOBAL, r, gidx, previous_.line);
        return r;
    }

    /* =========================================================
    ** Global slot management
    ** ========================================================= */

    int Compiler::require_global_slot(const char *name, int len)
    {
        char buf[256];
        int n = len < 255 ? len : 255;
        memcpy(buf, name, n);
        buf[n] = '\0';

        int gidx = vm_->find_global(buf);
        if (gidx >= 0)
            return gidx;
        return vm_->def_global(buf, val_nil());
    }

    int Compiler::find_or_add_global(const char *name, int len)
    {
        return require_global_slot(name, len);
    }

    /* =========================================================
    ** Helpers
    ** ========================================================= */

    bool Compiler::identifiers_equal(const Token &a, const Token &b) const
    {
        if (a.length != b.length)
            return false;
        return memcmp(a.start, b.start, a.length) == 0;
    }

    ObjString *Compiler::token_string(const Token &t)
    {
        return intern_string(gc_, t.start, t.length);
    }

    bool Compiler::is_declared_global(const Token &name) const
    {
        for (int i = 0; i < state_->global_count; i++)
        {
            if (identifiers_equal(state_->globals[i], name))
                return true;
        }
        return false;
    }

    /* =========================================================
    ** Operator precedence table
    ** ========================================================= */

    int Compiler::get_precedence(TokenType type) const
    {
        switch (type)
        {
        case TOK_IF:        return PREC_TERNARY;
        case TOK_OR:        return PREC_OR;
        case TOK_AND:       return PREC_AND;
        case TOK_EQEQ:
        case TOK_BANGEQ:
        case TOK_LT:
        case TOK_GT:
        case TOK_LTEQ:
        case TOK_GTEQ:
        case TOK_IS:
        case TOK_IN:        return PREC_COMPARISON;
        case TOK_PIPE:      return PREC_BITOR;
        case TOK_CARET:     return PREC_BITXOR;
        case TOK_AMP:       return PREC_BITAND;
        case TOK_LSHIFT:
        case TOK_RSHIFT:    return PREC_SHIFT;
        case TOK_PLUS:
        case TOK_MINUS:     return PREC_TERM;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_DSLASH:    return PREC_FACTOR;
        case TOK_DSTAR:     return PREC_POWER;
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_DOT:
        case TOK_QDOT:      return PREC_CALL;
        case TOK_DQMARK:    return PREC_OR; /* ?? at same level as 'or' */
        default:            return PREC_NONE;
        }
    }

    bool Compiler::is_right_associative(TokenType type) const
    {
        return type == TOK_DSTAR; /* ** is right-associative */
    }

    /* =========================================================
    ** Keyword arguments — the compile-time signature registry.
    **
    ** The parser is single-pass, so a call written before the `def` it
    ** targets has nothing to resolve against.  A cheap token-only
    ** pre-scan runs first and records every signature, which also makes
    ** `self.helper(x=1)` work regardless of where `helper` sits in the
    ** class body.
    ** ========================================================= */

    bool Compiler::sig_reserve(int extra_sigs, int extra_params)
    {
        if (sig_count_ + extra_sigs > sig_cap_)
        {
            int cap = sig_cap_ ? sig_cap_ * 2 : 64;
            while (cap < sig_count_ + extra_sigs)
                cap *= 2;
            FuncSig *n = (FuncSig *)zen_alloc_now(gc_, (size_t)cap * sizeof(FuncSig));
            if (!n)
                return false;
            if (sigs_)
            {
                memcpy(n, sigs_, (size_t)sig_count_ * sizeof(FuncSig));
                zen_free(gc_, sigs_, (size_t)sig_cap_ * sizeof(FuncSig));
            }
            sigs_ = n;
            sig_cap_ = cap;
        }
        if (sig_param_count_ + extra_params > sig_param_cap_)
        {
            int cap = sig_param_cap_ ? sig_param_cap_ * 2 : 256;
            while (cap < sig_param_count_ + extra_params)
                cap *= 2;
            SigParam *n = (SigParam *)zen_alloc_now(gc_, (size_t)cap * sizeof(SigParam));
            if (!n)
                return false;
            if (sig_params_)
            {
                memcpy(n, sig_params_, (size_t)sig_param_count_ * sizeof(SigParam));
                zen_free(gc_, sig_params_, (size_t)sig_param_cap_ * sizeof(SigParam));
            }
            sig_params_ = n;
            sig_param_cap_ = cap;
        }
        return true;
    }

    void Compiler::free_signatures()
    {
        if (sigs_)
            zen_free(gc_, sigs_, (size_t)sig_cap_ * sizeof(FuncSig));
        if (sig_params_)
            zen_free(gc_, sig_params_, (size_t)sig_param_cap_ * sizeof(SigParam));
        if (sig_classes_)
            zen_free(gc_, sig_classes_, (size_t)sig_class_cap_ * sizeof(SigClass));
        sigs_ = nullptr;
        sig_params_ = nullptr;
        sig_classes_ = nullptr;
        sig_count_ = sig_cap_ = 0;
        sig_param_count_ = sig_param_cap_ = 0;
        sig_class_count_ = sig_class_cap_ = 0;
    }

    static bool name_eq(const char *a, int32_t alen, const char *b, int32_t blen)
    {
        return alen == blen && memcmp(a, b, (size_t)alen) == 0;
    }

    const FuncSig *Compiler::find_signature(const char *owner, int owner_len,
                                            const char *name, int name_len) const
    {
        for (int i = 0; i < sig_count_; i++)
        {
            const FuncSig &s = sigs_[i];
            if (!name_eq(s.name, s.name_len, name, name_len))
                continue;
            if ((owner == nullptr) != (s.owner == nullptr))
                continue;
            if (owner && !name_eq(s.owner, s.owner_len, owner, owner_len))
                continue;
            return &s;
        }
        return nullptr;
    }

    const SigClass *Compiler::find_sig_class(const char *name, int name_len) const
    {
        for (int i = 0; i < sig_class_count_; i++)
            if (name_eq(sig_classes_[i].name, sig_classes_[i].name_len, name, name_len))
                return &sig_classes_[i];
        return nullptr;
    }

    /* Walk the token stream of one `def` header and record its parameters.
    ** Returns the first token after the closing ')'. */
    Token Compiler::scan_signature(Lexer &scan, const char *owner, int owner_len,
                                   const Token &name)
    {
        SigParam tmp[kMaxSigParams];
        int n = 0;
        int generic_count = 0;
        bool usable = true;

        Token t = scan.next_token();

        /* Generic type params: def f<T, U>(...). Counted separately from
        ** value params (generic_count), NOT stored in the value-param pool —
        ** a type argument at a call site is validated against generic_count,
        ** never mixed into keyword/positional value resolution. */
        if (t.type == TOK_LT)
        {
            for (;;)
            {
                t = scan.next_token();
                if (t.type != TOK_IDENTIFIER)
                {
                    usable = false;
                    break;
                }
                generic_count++;
                t = scan.next_token();
                if (t.type != TOK_COMMA)
                    break;
            }
            if (t.type == TOK_GT)
                t = scan.next_token();
            else
                usable = false;
        }

        if (t.type != TOK_LPAREN)
            return t; /* malformed — the real parser will report it */

        t = scan.next_token();
        bool first = true;
        while (t.type != TOK_RPAREN && t.type != TOK_EOF && t.type != TOK_ERROR)
        {
            if (t.type == TOK_STAR)
            {
                usable = false; /* *args: positions stop being fixed */
                break;
            }
            if (first && owner && t.type == TOK_SELF)
            {
                /* explicit `self` is not a parameter */
                t = scan.next_token();
                if (t.type == TOK_COMMA)
                    t = scan.next_token();
                first = false;
                continue;
            }
            if (t.type != TOK_IDENTIFIER)
            {
                usable = false;
                break;
            }

            SigParam p;
            p.name = t.start;
            p.name_len = t.length;
            p.has_default = false;
            p.default_negate = false;
            p.default_tok = t;

            t = scan.next_token();

            /* Type hint: skip to the ',', '=' or ')' that closes it.  The
            ** hint may itself carry brackets (`list[int]`). */
            if (t.type == TOK_COLON)
            {
                int bracket = 0;
                t = scan.next_token();
                while (t.type != TOK_EOF && t.type != TOK_ERROR)
                {
                    if (t.type == TOK_LBRACKET || t.type == TOK_LPAREN)
                        bracket++;
                    else if (t.type == TOK_RBRACKET)
                        bracket--;
                    else if (t.type == TOK_RPAREN)
                    {
                        if (bracket == 0)
                            break;
                        bracket--;
                    }
                    else if (bracket == 0 && (t.type == TOK_COMMA || t.type == TOK_EQ))
                        break;
                    t = scan.next_token();
                }
            }

            if (t.type == TOK_EQ)
            {
                t = scan.next_token();
                if (t.type == TOK_MINUS)
                {
                    p.default_negate = true;
                    t = scan.next_token();
                }
                p.has_default = true;
                p.default_tok = t;
                t = scan.next_token();
            }

            if (n < kMaxSigParams)
                tmp[n] = p;
            n++;
            first = false;

            if (t.type != TOK_COMMA)
                break;
            t = scan.next_token();
        }

        /* Skip whatever is left of the header (return hint, stray tokens). */
        while (t.type != TOK_RPAREN && t.type != TOK_EOF && t.type != TOK_ERROR &&
               t.type != TOK_NEWLINE)
            t = scan.next_token();
        if (t.type == TOK_RPAREN)
            t = scan.next_token();

        /* `-> Self` / `-> OwnClass` on a method: it returns its receiver. */
        bool returns_self = false;
        if (t.type == TOK_ARROW)
        {
            t = scan.next_token();
            if (owner && t.type == TOK_IDENTIFIER &&
                ((t.length == 4 && memcmp(t.start, "Self", 4) == 0) ||
                 name_eq(t.start, t.length, owner, owner_len)))
                returns_self = true;
        }

        if (n > kMaxSigParams)
            usable = false;

        /* A second `def` under the same key makes the position of a name
        ** unknowable — refuse keywords on both rather than guess. */
        for (int i = 0; i < sig_count_; i++)
        {
            FuncSig &e = sigs_[i];
            if (!name_eq(e.name, e.name_len, name.start, name.length))
                continue;
            if ((owner == nullptr) != (e.owner == nullptr))
                continue;
            if (owner && !name_eq(e.owner, e.owner_len, owner, owner_len))
                continue;
            e.takes_keywords = false;
            e.returns_self = false;
            return t;
        }

        int keep = n > kMaxSigParams ? kMaxSigParams : n;
        if (!sig_reserve(1, keep))
            return t;

        FuncSig sig;
        sig.owner = owner;
        sig.owner_len = owner_len;
        sig.name = name.start;
        sig.name_len = name.length;
        sig.param_start = sig_param_count_;
        sig.param_count = keep;
        sig.generic_count = generic_count;
        sig.takes_keywords = usable;
        sig.returns_self = returns_self;
        for (int i = 0; i < keep; i++)
            sig_params_[sig_param_count_++] = tmp[i];
        sigs_[sig_count_++] = sig;
        return t;
    }

    void Compiler::prescan_signatures(const char *source, const char *filename)
    {
        Lexer scan;
        scan.init(source, filename);

        /* Innermost classes and the depth their bodies live at. */
        static const int kMaxClassNest = 8;
        struct { const char *name; int32_t len; int body_depth; } stack[kMaxClassNest];
        int nest = 0;
        int depth = 0;

        /* "Returns self" analysis of the method body being scanned: every
        ** `return` in it must be literally `return self`, and the body's
        ** last statement at its own depth must be one too — then every
        ** path hands the receiver back. `yield`/`await` (the call does not
        ** produce the value) or a rebinding of `self` disqualify. Nested
        ** defs only ever make the answer more conservative. */
        int body_sig = -1;
        int body_depth = 0;
        bool body_all_self = true;
        bool body_last_self = false;
        bool body_any_return = false;
        TokenType prev_type = TOK_NEWLINE;

        Token t = scan.next_token();
        while (t.type != TOK_EOF && t.type != TOK_ERROR)
        {
            if (t.type == TOK_INDENT)
            {
                depth++;
                prev_type = TOK_INDENT;
                t = scan.next_token();
                continue;
            }
            if (t.type == TOK_DEDENT)
            {
                if (depth > 0)
                    depth--;
                while (nest > 0 && depth < stack[nest - 1].body_depth)
                    nest--;
                if (body_sig >= 0 && depth < body_depth)
                {
                    if (body_any_return && body_all_self && body_last_self)
                        sigs_[body_sig].returns_self = true;
                    body_sig = -1;
                }
                prev_type = TOK_DEDENT;
                t = scan.next_token();
                continue;
            }
            if (body_sig >= 0 && depth >= body_depth)
            {
                bool stmt_start = prev_type == TOK_NEWLINE || prev_type == TOK_INDENT ||
                                  prev_type == TOK_DEDENT || prev_type == TOK_SEMICOLON;
                bool at_body_level = stmt_start && depth == body_depth;
                if (t.type == TOK_RETURN)
                {
                    body_any_return = true;
                    Token n1 = scan.next_token();
                    if (n1.type == TOK_SELF)
                    {
                        Token n2 = scan.next_token();
                        bool is_self = n2.type == TOK_NEWLINE || n2.type == TOK_EOF ||
                                       n2.type == TOK_DEDENT || n2.type == TOK_SEMICOLON;
                        if (!is_self)
                            body_all_self = false;
                        if (at_body_level)
                            body_last_self = is_self;
                        prev_type = TOK_SELF;
                        t = n2;
                        continue;
                    }
                    body_all_self = false;
                    if (at_body_level)
                        body_last_self = false;
                    prev_type = TOK_RETURN;
                    t = n1;
                    continue;
                }
                if (t.type == TOK_YIELD || t.type == TOK_AWAIT)
                    body_all_self = false;
                if (at_body_level)
                    body_last_self = false;
                if (stmt_start && t.type == TOK_SELF)
                {
                    Token n1 = scan.next_token();
                    if (n1.type == TOK_EQ)
                        body_all_self = false;
                    prev_type = TOK_SELF;
                    t = n1;
                    continue;
                }
            }
            if (t.type == TOK_CLASS)
            {
                Token cname = scan.next_token();
                if (cname.type != TOK_IDENTIFIER)
                {
                    t = cname;
                    continue;
                }
                const char *parent = nullptr;
                int32_t parent_len = 0;
                t = scan.next_token();
                if (t.type == TOK_LPAREN)
                {
                    Token pt = scan.next_token();
                    if (pt.type == TOK_IDENTIFIER)
                    {
                        parent = pt.start;
                        parent_len = pt.length;
                    }
                    while (t.type != TOK_RPAREN && t.type != TOK_EOF && t.type != TOK_NEWLINE)
                        t = scan.next_token();
                }
                if (nest < kMaxClassNest)
                {
                    stack[nest].name = cname.start;
                    stack[nest].len = cname.length;
                    stack[nest].body_depth = depth + 1;
                    nest++;
                }
                if (sig_class_count_ + 1 > sig_class_cap_)
                {
                    int cap = sig_class_cap_ ? sig_class_cap_ * 2 : 32;
                    SigClass *nc = (SigClass *)zen_alloc_now(gc_, (size_t)cap * sizeof(SigClass));
                    if (nc)
                    {
                        if (sig_classes_)
                        {
                            memcpy(nc, sig_classes_, (size_t)sig_class_count_ * sizeof(SigClass));
                            zen_free(gc_, sig_classes_, (size_t)sig_class_cap_ * sizeof(SigClass));
                        }
                        sig_classes_ = nc;
                        sig_class_cap_ = cap;
                    }
                }
                if (sig_class_count_ < sig_class_cap_)
                {
                    SigClass &c = sig_classes_[sig_class_count_++];
                    c.name = cname.start;
                    c.name_len = cname.length;
                    c.parent = parent;
                    c.parent_len = parent_len;
                }
                prev_type = TOK_IDENTIFIER;
                continue;
            }
            if (t.type == TOK_DEF)
            {
                bool is_async = prev_type == TOK_ASYNC;
                Token fname = scan.next_token();
                if (fname.type != TOK_IDENTIFIER)
                {
                    prev_type = TOK_DEF;
                    t = fname;
                    continue;
                }
                bool is_method = nest > 0 && depth == stack[nest - 1].body_depth;
                bool is_free = depth == 0;
                if (is_method)
                {
                    int before = sig_count_;
                    t = scan_signature(scan, stack[nest - 1].name, stack[nest - 1].len, fname);
                    /* A duplicate def (nothing appended) or a coroutine is
                    ** left unknown. */
                    body_sig = (sig_count_ > before && !is_async) ? sig_count_ - 1 : -1;
                    body_depth = depth + 1;
                    body_all_self = true;
                    body_last_self = false;
                    body_any_return = false;
                }
                else if (is_free)
                    t = scan_signature(scan, nullptr, 0, fname);
                else
                    t = scan.next_token(); /* nested def: not addressable by name */
                prev_type = TOK_IDENTIFIER;
                continue;
            }
            prev_type = t.type;
            t = scan.next_token();
        }
    }

    /* --- Resolving a call site to a signature --- */

    bool Compiler::receiver_class(int reg, const char *&name, int32_t &len, bool *exact) const
    {
        if (exact)
            *exact = false;
        if (state_->is_method && in_class_ && reg == 0)
        {
            name = current_class_.start;
            len = current_class_.length;
            return true; /* self may be a subclass instance: not exact */
        }
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].reg == reg && state_->locals[i].has_type_hint)
            {
                name = state_->locals[i].type_hint.start;
                len = state_->locals[i].type_hint.length;
                if (exact)
                    *exact = state_->locals[i].type_exact;
                return true;
            }
        }
        return false;
    }

    void Compiler::skip_nullable_suffix()
    {
        if (match(TOK_QMARK))
            return;
        if (check(TOK_PIPE))
        {
            advance();
            consume(TOK_NONE, "Expected 'None' after '|' in a type annotation.");
        }
    }

    void Compiler::set_local_type_hint(int reg, const Token &type_tok)
    {
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            if (state_->locals[i].reg == reg)
            {
                state_->locals[i].has_type_hint = true;
                state_->locals[i].type_hint = type_tok;
                state_->locals[i].type_inferred = false;
                state_->locals[i].type_exact = false;
                return;
            }
        }
    }

    void Compiler::set_global_type_hint(int gidx, const Token &type_tok)
    {
        /* A second annotation for the same global (re-annotated, or a
        ** second `def`-like redeclaration) replaces the first rather than
        ** growing the table — the most recent annotation wins, same as
        ** re-assigning any other compile-time fact about a name. */
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            if (global_type_hints_[i].gidx == gidx)
            {
                global_type_hints_[i].type_tok = type_tok;
                global_type_hints_[i].has_class_type = true;
                global_type_hints_[i].inferred = false;
                global_type_hints_[i].exact = false;
                return;
            }
        }
        if (global_type_hint_count_ < kMaxGlobalTypeHints)
        {
            global_type_hints_[global_type_hint_count_].gidx = gidx;
            global_type_hints_[global_type_hint_count_].type_tok = type_tok;
            global_type_hints_[global_type_hint_count_].has_class_type = true;
            global_type_hints_[global_type_hint_count_].has_array_element_type = false;
            global_type_hints_[global_type_hint_count_].inferred = false;
            global_type_hints_[global_type_hint_count_].exact = false;
            global_type_hint_count_++;
        }
        /* Table full: silently not tracked — worst case obj.method<T>(...)
        ** on this particular global falls back to being read as a plain
        ** comparison, same as any other receiver with no known type. */
    }

    bool Compiler::global_type_hint(int gidx, const char *&name, int32_t &len, bool *exact) const
    {
        if (exact)
            *exact = false;
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            if (global_type_hints_[i].gidx == gidx && global_type_hints_[i].has_class_type)
            {
                name = global_type_hints_[i].type_tok.start;
                len = global_type_hints_[i].type_tok.length;
                if (exact)
                    *exact = global_type_hints_[i].exact;
                return true;
            }
        }
        return false;
    }

    void Compiler::set_local_array_element_type(int reg, const Token &type_tok)
    {
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            if (state_->locals[i].reg == reg)
            {
                state_->locals[i].has_array_element_type = true;
                state_->locals[i].array_element_type = type_tok;
                return;
            }
        }
    }

    void Compiler::set_global_array_element_type(int gidx, const Token &type_tok)
    {
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            if (global_type_hints_[i].gidx == gidx)
            {
                global_type_hints_[i].has_array_element_type = true;
                global_type_hints_[i].array_element_type = type_tok;
                return;
            }
        }
        if (global_type_hint_count_ < kMaxGlobalTypeHints)
        {
            GlobalTypeHint &hint = global_type_hints_[global_type_hint_count_++];
            hint.gidx = gidx;
            hint.has_class_type = false;
            hint.has_array_element_type = true;
            hint.array_element_type = type_tok;
        }
    }

    bool Compiler::array_element_class(int reg, const char *&name, int32_t &len) const
    {
        for (int i = 0; i < state_->local_count; i++)
        {
            const Local &local = state_->locals[i];
            if (local.reg == reg && local.has_array_element_type)
            {
                name = local.array_element_type.start;
                len = local.array_element_type.length;
                return true;
            }
        }
        if (!pending_subscript_receiver_valid_)
            return false;

        /* The bare array may be an upvalue; use its nearest lexical binding. */
        for (CompilerState *s = state_; s != nullptr; s = s->parent)
        {
            for (int i = s->local_count - 1; i >= 0; i--)
            {
                const Local &local = s->locals[i];
                if (identifiers_equal(local.name, pending_subscript_receiver_))
                {
                    if (!local.has_array_element_type)
                        return false;
                    name = local.array_element_type.start;
                    len = local.array_element_type.length;
                    return true;
                }
            }
        }
        char buf[256];
        int n = pending_subscript_receiver_.length < 255 ? pending_subscript_receiver_.length : 255;
        memcpy(buf, pending_subscript_receiver_.start, n);
        buf[n] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return false;
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            const GlobalTypeHint &hint = global_type_hints_[i];
            if (hint.gidx == gidx && hint.has_array_element_type)
            {
                name = hint.array_element_type.start;
                len = hint.array_element_type.length;
                return true;
            }
        }
        return false;
    }

    int Compiler::registry_field_index(const char *cls, int32_t cls_len, ObjString *fname) const
    {
        /* The class being compiled is not in the registry yet: its fields
        ** seen so far are in class_field_table_. */
        if (in_class_ && name_eq(current_class_.start, current_class_.length, cls, cls_len))
            return lookup_class_field(fname);
        for (int i = 0; i < class_registry_count_; i++)
        {
            const ClassFieldRegistry &r = class_registry_[i];
            if (!name_eq(r.name.start, r.name.length, cls, cls_len))
                continue;
            for (int j = 0; j < r.count; j++)
                if (r.fields[j] == fname)
                    return j;
            return -1;
        }
        return -1;
    }

    void Compiler::note_field_class(int fidx, bool rhs_is_none)
    {
        if (fidx < 0 || fidx >= kMaxClassFields)
            return;
        uint8_t &st = class_field_class_state_[fidx];
        if (st == 3)
            return; /* declared in the class body: the annotation wins */
        if (last_expr_ctor_valid_)
        {
            if (st == 0)
            {
                class_field_class_[fidx] = last_expr_ctor_class_;
                st = 1;
            }
            else if (st == 1 && !identifiers_equal(class_field_class_[fidx], last_expr_ctor_class_))
                st = 2;
        }
        else if (!rhs_is_none)
            st = 2;
    }

    bool Compiler::field_class_guess(const char *cls, int32_t cls_len, int fidx, Token &out) const
    {
        if (fidx < 0 || fidx >= kMaxClassFields)
            return false;
        if (in_class_ && name_eq(current_class_.start, current_class_.length, cls, cls_len))
        {
            if (fidx >= class_field_count_ ||
                (class_field_class_state_[fidx] != 1 && class_field_class_state_[fidx] != 3))
                return false;
            out = class_field_class_[fidx];
            return true;
        }
        for (int i = 0; i < class_registry_count_; i++)
        {
            const ClassFieldRegistry &r = class_registry_[i];
            if (!name_eq(r.name.start, r.name.length, cls, cls_len))
                continue;
            if (fidx >= r.count || (r.field_class_state[fidx] != 1 && r.field_class_state[fidx] != 3))
                return false;
            out = r.field_class[fidx];
            return true;
        }
        return false;
    }

    bool Compiler::receiver_static_class(int reg, const char *&name, int32_t &len, bool *exact) const
    {
        if (exact)
            *exact = false;
        if (reg == typed_subscript_reg_)
        {
            name = typed_subscript_class_.start;
            len = typed_subscript_class_.length;
            return true;
        }
        if (reg == typed_call_reg_)
        {
            name = typed_call_class_.start;
            len = typed_call_class_.length;
            if (exact)
                *exact = typed_call_exact_;
            return true;
        }
        if (receiver_class(reg, name, len, exact))
            return true;
        /* Fall back to a global's class-type annotation, ONLY when this
        ** dot_expr's receiver is literally the bare name the Pratt loop
        ** just saw (pending_receiver_) — a temporary/expression result in
        ** the same register has no such name and must not borrow one. */
        if (!pending_receiver_valid_)
            return false;
        /* A bare receiver can be an upvalue. Find the nearest matching local
        ** in the lexical chain without calling resolve_upvalue() (which has
        ** side effects). Its annotation remains true after the closure has
        ** captured it; importantly, an unannotated nearer local still
        ** shadows a similarly named annotated global. */
        for (CompilerState *s = state_; s != nullptr; s = s->parent)
        {
            for (int i = s->local_count - 1; i >= 0; i--)
            {
                if (identifiers_equal(s->locals[i].name, pending_receiver_))
                {
                    if (!s->locals[i].has_type_hint)
                        return false;
                    name = s->locals[i].type_hint.start;
                    len = s->locals[i].type_hint.length;
                    if (exact)
                        *exact = s->locals[i].type_exact;
                    return true;
                }
            }
        }
        char buf[256];
        int n = pending_receiver_.length < 255 ? pending_receiver_.length : 255;
        memcpy(buf, pending_receiver_.start, n);
        buf[n] = '\0';
        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return false;
        return global_type_hint(gidx, name, len, exact);
    }

    bool Compiler::native_generic_method_arity(const char *cls_name, int32_t cls_len,
                                               const Token &method, int &out_arity) const
    {
        char buf[256];
        int n = cls_len < 255 ? cls_len : 255;
        memcpy(buf, cls_name, n);
        buf[n] = '\0';

        int gidx = vm_->find_global(buf);
        if (gidx < 0)
            return false;
        Value cls_val = vm_->get_global(gidx);
        if (!is_class(cls_val))
            return false;
        ObjClass *klass = as_class(cls_val);

        int slot = vm_->find_selector(method.start, method.length);
        if (slot < 0)
            return false;

        for (ObjClass *search = klass; search != nullptr; search = search->parent)
        {
            if (slot >= search->vtable_size)
                continue;
            Value mval = search->vtable[slot];
            if (is_nil(mval))
                continue;
            if (!is_native(mval))
                return false; /* a script method shadows/overrides — not this path */
            ObjNative *nat = as_native(mval);
            if (nat->generic_arity <= 0)
                return false;
            out_arity = nat->generic_arity;
            return true;
        }
        return false;
    }

    /* Look up `name` on `owner`, then on what `owner` inherits from. */
    static const int kMaxSigInherit = 8;

    /* Fallback for a receiver with no static type: if exactly one class in
    ** the file declares a method by this name, its parameter names are the
    ** only ones a keyword could have meant.  Two classes declaring it makes
    ** the position of a name unknowable, and the call site says so rather
    ** than picking one. */
    const FuncSig *Compiler::unique_method_signature(const Token &method) const
    {
        const FuncSig *found = nullptr;
        for (int i = 0; i < sig_count_; i++)
        {
            const FuncSig &s = sigs_[i];
            if (!s.owner || !name_eq(s.name, s.name_len, method.start, method.length))
                continue;
            if (found)
                return nullptr; /* ambiguous */
            found = &s;
        }
        return found;
    }

    const FuncSig *Compiler::method_signature(int recv_reg, const Token &method) const
    {
        const char *cls = nullptr;
        int32_t cls_len = 0;
        if (!receiver_class(recv_reg, cls, cls_len))
            return unique_method_signature(method);

        for (int hop = 0; hop < kMaxSigInherit && cls; hop++)
        {
            const FuncSig *s = find_signature(cls, cls_len, method.start, method.length);
            if (s)
                return s;
            const SigClass *c = find_sig_class(cls, cls_len);
            if (!c || !c->parent)
                return unique_method_signature(method);
            cls = c->parent;
            cls_len = c->parent_len;
        }
        return nullptr;
    }

    const FuncSig *Compiler::super_signature(const Token &method) const
    {
        if (!in_class_ || !class_has_parent_)
            return nullptr;
        const char *cls = current_class_parent_.start;
        int32_t cls_len = current_class_parent_.length;
        for (int hop = 0; hop < kMaxSigInherit && cls; hop++)
        {
            const FuncSig *s = find_signature(cls, cls_len, method.start, method.length);
            if (s)
                return s;
            const SigClass *c = find_sig_class(cls, cls_len);
            if (!c || !c->parent)
                return nullptr;
            cls = c->parent;
            cls_len = c->parent_len;
        }
        return nullptr;
    }

    /* A bare `name(...)`: either a free function, or a class, in which case
    ** the arguments are its init()'s. */
    const FuncSig *Compiler::callee_signature()
    {
        if (!pending_callee_valid_)
            return nullptr;
        const Token &n = pending_callee_;

        /* A local or upvalue of the same name shadows the declaration the
        ** pre-scan saw, and we cannot know what it holds. */
        if (resolve_local(state_, n) != -1)
            return nullptr;

        const FuncSig *s = find_signature(nullptr, 0, n.start, n.length);
        if (s)
            return s;

        if (find_sig_class(n.start, n.length))
        {
            const char *cls = n.start;
            int32_t cls_len = n.length;
            for (int hop = 0; hop < kMaxSigInherit && cls; hop++)
            {
                const FuncSig *init = find_signature(cls, cls_len, "__init__", 8);
                if (init)
                    return init;
                const SigClass *c = find_sig_class(cls, cls_len);
                if (!c || !c->parent)
                    return nullptr;
                cls = c->parent;
                cls_len = c->parent_len;
            }
        }
        return nullptr;
    }

    const FuncSig *Compiler::find_method_in_chain(const char *cls, int32_t cls_len,
                                                  const Token &method) const
    {
        for (int hop = 0; hop < kMaxSigInherit && cls; hop++)
        {
            const FuncSig *s = find_signature(cls, cls_len, method.start, method.length);
            if (s)
                return s;
            const SigClass *c = find_sig_class(cls, cls_len);
            if (!c || !c->parent)
                return nullptr;
            cls = c->parent;
            cls_len = c->parent_len;
        }
        return nullptr;
    }

    /* Does any class in this file that descends from `cls` declare `method`
    ** itself? If so, a receiver that is only known to be *some* `cls` may
    ** run the override, and nothing the base declaration promises (such as
    ** returning self) can be assumed of it. */
    bool Compiler::method_overridden_below(const char *cls, int32_t cls_len,
                                           const Token &method) const
    {
        for (int i = 0; i < sig_class_count_; i++)
        {
            const SigClass &c = sig_classes_[i];
            if (name_eq(c.name, c.name_len, cls, cls_len))
                continue;
            const char *p = c.parent;
            int32_t plen = c.parent_len;
            bool descends = false;
            for (int hop = 0; hop < kMaxSigInherit && p; hop++)
            {
                if (name_eq(p, plen, cls, cls_len))
                {
                    descends = true;
                    break;
                }
                const SigClass *pc = find_sig_class(p, plen);
                if (!pc)
                    break;
                p = pc->parent;
                plen = pc->parent_len;
            }
            if (descends && find_signature(c.name, c.name_len, method.start, method.length))
                return true;
        }
        return false;
    }

    /* A bare name that is a class declared in this file, shadowed by no
    ** local or enclosing local, and not also the name of a free def. */
    bool Compiler::known_script_class(const Token &name) const
    {
        for (CompilerState *s = state_; s != nullptr; s = s->parent)
        {
            for (int i = s->local_count - 1; i >= 0; i--)
                if (identifiers_equal(s->locals[i].name, name))
                    return false;
        }
        if (find_signature(nullptr, 0, name.start, name.length))
            return false;
        return find_sig_class(name.start, name.length) != nullptr;
    }

    void Compiler::infer_assigned_class_local(int reg)
    {
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            Local &l = state_->locals[i];
            if (l.reg != reg)
                continue;
            if (l.has_type_hint && !l.type_inferred)
                return; /* an annotation is the user's promise; keep it */
            if (last_expr_ctor_valid_ && !l.captured)
            {
                l.has_type_hint = true;
                l.type_hint = last_expr_ctor_class_;
                l.type_inferred = true;
                l.type_exact = true;
            }
            else
            {
                l.has_type_hint = false;
                l.type_inferred = false;
                l.type_exact = false;
            }
            return;
        }
    }

    void Compiler::infer_assigned_class_global(int gidx)
    {
        bool written_in_fn = false;
        for (int i = 0; i < fn_written_global_count_; i++)
            if (fn_written_globals_[i] == gidx)
                written_in_fn = true;
        bool infer = last_expr_ctor_valid_ && !written_in_fn;
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            GlobalTypeHint &h = global_type_hints_[i];
            if (h.gidx != gidx)
                continue;
            if (h.has_class_type && !h.inferred)
                return;
            h.has_class_type = infer;
            h.inferred = infer;
            h.exact = infer;
            if (infer)
                h.type_tok = last_expr_ctor_class_;
            return;
        }
        if (!infer || global_type_hint_count_ >= kMaxGlobalTypeHints)
            return;
        GlobalTypeHint &h = global_type_hints_[global_type_hint_count_++];
        h.gidx = gidx;
        h.type_tok = last_expr_ctor_class_;
        h.has_class_type = true;
        h.has_array_element_type = false;
        h.inferred = true;
        h.exact = true;
    }

    void Compiler::note_global_written_in_function(int gidx)
    {
        if (state_->parent == nullptr)
            return;
        for (int i = 0; i < fn_written_global_count_; i++)
            if (fn_written_globals_[i] == gidx)
                return;
        if (fn_written_global_count_ < kMaxFnWrittenGlobals)
            fn_written_globals_[fn_written_global_count_++] = gidx;
        for (int i = 0; i < global_type_hint_count_; i++)
        {
            GlobalTypeHint &h = global_type_hints_[i];
            if (h.gidx == gidx && h.inferred)
            {
                h.has_class_type = false;
                h.inferred = false;
                h.exact = false;
            }
        }
    }

    int Compiler::sig_param_index(const FuncSig *sig, const Token &name) const
    {
        for (int i = 0; i < sig->param_count; i++)
        {
            const SigParam &p = sig_params_[sig->param_start + i];
            if (name_eq(p.name, p.name_len, name.start, name.length))
                return i;
        }
        return -1;
    }

    void Compiler::emit_sig_default(const SigParam &p, int reg)
    {
        switch (p.default_tok.type)
        {
        case TOK_INT:
        {
            int64_t v = strtoll(p.default_tok.start, nullptr, 0);
            if (p.default_negate)
                v = -v;
            if (v >= -32768 && v <= 32767)
                state_->emitter.emit_asbx(OP_LOADI, reg, (int)v, p.default_tok.line);
            else
            {
                int ki = state_->emitter.add_constant(val_int(v));
                state_->emitter.emit_abx(OP_LOADK, reg, ki, p.default_tok.line);
            }
            break;
        }
        case TOK_FLOAT:
        {
            double v = strtod(p.default_tok.start, nullptr);
            if (p.default_negate)
                v = -v;
            int ki = state_->emitter.add_constant(val_float(v));
            state_->emitter.emit_abx(OP_LOADK, reg, ki, p.default_tok.line);
            break;
        }
        case TOK_STRING:
            string_literal(p.default_tok, reg);
            break;
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_NONE:
            literal(p.default_tok, reg);
            break;
        default:
            error("Default value of skipped parameter is not a literal.");
            state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, p.default_tok.line);
            break;
        }
    }

} /* namespace zen */
