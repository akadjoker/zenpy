/*
** compiler_expressions.cpp — Pratt expression parser.
**
** Handles: number, string, identifier, unary, binary, comparisons,
** logical and/or, function calls, subscript, dot access, array/map literals.
*/

#include "compiler.h"

namespace zen
{

    /* =========================================================
    ** Top-level expression entry point
    ** ========================================================= */

    int Compiler::expression(int dest)
    {
        return parse_precedence(PREC_ASSIGNMENT + 1, dest);
    }

    /* =========================================================
    ** Pratt parser: precedence climbing
    ** ========================================================= */

    /* All expression recursion funnels through here — nested parens,
    ** brackets, braces, calls, everything. One depth guard at the funnel
    ** turns what was a C stack overflow (SIGSEGV) into a clean compile
    ** error. 200 is far beyond hand-written code and far below the stack:
    ** the wasm build crashed at ~6000 frames on an 8MB stack. */
    static constexpr int kMaxExprDepth = 200;

    int Compiler::parse_precedence(int prec, int dest)
    {
        /* Once an error is raised, do not descend again. Several literals
        ** (map/set, comprehensions) roll the lexer back and re-parse the
        ** first sub-expression a different way; on malformed input that
        ** retry, driven off a failed parse, is what turned `{{{{...` into an
        ** exponential/endless reparse. Bailing here stops it — the compiler
        ** discards the result on had_error_ regardless. */
        if (panic_mode_ || abort_parse_)
            return dest >= 0 ? dest : alloc_reg();
        if (++expr_depth_ > kMaxExprDepth)
        {
            expr_depth_--;
            error("Expression nested too deeply.");
            abort_parse_ = true; /* unrecoverable: stop the whole parse */
            return dest >= 0 ? dest : alloc_reg();
        }
        int reg = parse_precedence_inner(prec, dest);
        expr_depth_--;
        return reg;
    }

    int Compiler::parse_precedence_inner(int prec, int dest)
    {
        advance();
        Token token = previous_;

        /* Prefix */
        int reg = prefix_rule(token, dest);
        if (had_error_)
            return reg;

        /* `name(` is the only shape whose callee still has a name at the
        ** point call_expr() runs — remember it for keyword resolution. */
        bool bare_name = token.type == TOK_IDENTIFIER;

        /* Infix — keep parsing while the next operator binds tighter */
        for (;;)
        {
            /* Generic call: fn<Type>(args).  Only treat '<' as generic syntax
            ** when (a) its complete shape is <Identifier[, Identifier]*>( and
            ** (b) `fn` is a name the compiler already knows is a def/method —
            ** never a plain variable.  (b) is what keeps `f<T, U>(h)` from
            ** being read as a generic call when `f` merely happens to hold an
            ** int: without it, punctuation alone can't tell a real generic
            ** call apart from `f < T, U > (h)` (two chained comparisons in a
            ** parenthesized tuple written without spaces by habit). */
            if (current_.type == TOK_LT && bare_name)
            {
                pending_callee_valid_ = true;
                pending_callee_ = token;
                const FuncSig *maybe_sig = callee_signature();
                pending_callee_valid_ = false;
                if (generic_call_ahead(maybe_sig != nullptr))
                {
                    pending_callee_valid_ = true;
                    pending_callee_ = token;
                    bare_name = false;
                    reg = generic_call_expr(reg, dest);
                    pending_callee_valid_ = false;
                    if (had_error_)
                        return reg;
                    continue;
                }
            }

            int infix_prec = get_precedence(current_.type);

            /* Special case: 'if' on a new line is a statement, not ternary operator */
            if (current_.type == TOK_IF && current_.line > previous_.line)
            {
                break;
            }

            /* 'not in' compound operator at PREC_COMPARISON */
            if (current_.type == TOK_NOT && infix_prec == PREC_NONE)
            {
                /* Peek if next would be 'in' — treat as PREC_COMPARISON */
                LexerState ls = lexer_.save_state();
                Token saved_cur = current_;
                advance(); /* consume 'not' */
                if (current_.type == TOK_IN && PREC_COMPARISON >= prec)
                {
                    /* 'not in' is the operator — hand to infix_rule with op=TOK_NOT */
                    Token op = previous_; /* the 'not' token */
                    reg = infix_rule(op, reg, dest);
                    if (had_error_)
                        return reg;
                    continue;
                }
                /* Not 'not in' — restore and break */
                lexer_.restore_state(ls);
                current_ = saved_cur;
                break;
            }
            if (infix_prec < prec)
                break;
            advance();
            Token op = previous_;
            pending_callee_valid_ = bare_name && op.type == TOK_LPAREN;
            pending_callee_ = token;
            /* `name.field` — remember the bare name so dot_expr() can look
            ** it up in the global class-type-hint table when it isn't a
            ** local with a type hint (receiver_class() only knows locals).
            ** Same idea as pending_callee_, one hop earlier. */
            pending_receiver_valid_ = bare_name && op.type == TOK_DOT;
            pending_receiver_ = token;
            pending_subscript_receiver_valid_ = bare_name && op.type == TOK_LBRACKET;
            pending_subscript_receiver_ = token;
            bare_name = false;
            reg = infix_rule(op, reg, dest);
            pending_callee_valid_ = false;
            pending_receiver_valid_ = false;
            pending_subscript_receiver_valid_ = false;
            if (had_error_)
                return reg;
        }

        return reg;
    }

    /* =========================================================
    ** Prefix rules
    ** ========================================================= */

    int Compiler::prefix_rule(Token token, int dest)
    {
        switch (token.type)
        {
        case TOK_INT:
        case TOK_FLOAT:
            return number(token, dest);
        case TOK_STRING:
            return string_literal(token, dest);
        case TOK_FSTRING:
            return fstring_literal(token, dest);
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_NONE:
            return literal(token, dest);
        case TOK_IDENTIFIER:
        case TOK_UNDERSCORE:
            return variable(token, dest, true);
        case TOK_SELF:
            return variable(token, dest, false);
        case TOK_SUPER:
            return super_expr(dest);
        case TOK_MINUS:
        case TOK_TILDE:
        case TOK_NOT:
            return unary(token, dest);
        case TOK_LPAREN:
        {
            /* Empty tuple: () */
            if (check(TOK_RPAREN))
            {
                advance();
                int reg = (dest >= 0) ? dest : alloc_reg();
                state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, token.line);
                return reg;
            }
            int r = expression(dest);
            /* Tuple: (expr, ...) — comma after first expr → build array */
            if (check(TOK_COMMA))
            {
                /* Array reg must differ from r to avoid overwriting first value */
                int reg = alloc_reg();
                state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, token.line);
                state_->emitter.emit_abc(OP_APPEND, reg, r, 0, token.line);
                while (match(TOK_COMMA))
                {
                    if (check(TOK_RPAREN))
                        break; /* trailing comma */
                    int elem = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, elem, 0, previous_.line);
                    free_reg(elem);
                }
                consume(TOK_RPAREN, "Expected ')' after tuple.");
                if (dest >= 0 && dest != reg)
                {
                    emit_move(dest, reg);
                    free_reg(reg);
                    return dest;
                }
                return reg;
            }
            consume(TOK_RPAREN, "Expected ')' after expression.");
            return r;
        }
        case TOK_LBRACKET:
            return array_literal(dest);
        case TOK_LBRACE:
            return map_literal(dest);
        case TOK_LAMBDA:
            return lambda_expr(dest);
        case TOK_YIELD:
            return yield_expr(dest);
        case TOK_AWAIT:
            return await_expr(dest);
        default:
            error("Expected expression.");
            return dest >= 0 ? dest : alloc_reg();
        }
    }

    /* =========================================================
    ** Infix rules
    ** ========================================================= */

    int Compiler::infix_rule(Token op, int left, int dest)
    {
        switch (op.type)
        {
        /* Arithmetic / bitwise */
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_DSLASH:
        case TOK_DSTAR:
        case TOK_AMP:
        case TOK_PIPE:
        case TOK_CARET:
        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return binary(op, left, dest);

        /* Comparison */
        case TOK_EQEQ:
        case TOK_BANGEQ:
        case TOK_LT:
        case TOK_GT:
        case TOK_LTEQ:
        case TOK_GTEQ:
        case TOK_IS:
        case TOK_IN:
        case TOK_NOT:
            return comparison(op, left, dest);

        /* Logical */
        case TOK_AND:
            return logical_and(left, dest);
        case TOK_OR:
            return logical_or(left, dest);
        case TOK_IF:
            return ternary_expr(left, dest);

        /* Postfix-like (call, subscript, dot) */
        case TOK_LPAREN:
            return call_expr(left, dest);
        case TOK_LBRACKET:
            return subscript_expr(left, dest, true);
        case TOK_DOT:
            return dot_expr(left, dest, true);
        case TOK_QDOT:
            return safe_dot_expr(left, dest);
        case TOK_DQMARK:
            return null_coalesce(left, dest);

        default:
            error("Unexpected infix operator.");
            return left;
        }
    }

    /* =========================================================
    ** Number literal
    ** ========================================================= */

    int Compiler::number(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        if (token.type == TOK_INT)
        {
            /* Try small integer (fits in sBx = signed 16-bit) */
            int64_t val = strtoll(token.start, nullptr, 0);
            if (val >= -32768 && val <= 32767)
            {
                state_->emitter.emit_asbx(OP_LOADI, reg, (int)val, token.line);
                return reg;
            }
            /* Large int → constant pool */
            int ki = state_->emitter.add_constant(val_int(val));
            state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
            return reg;
        }

        /* Float */
        double val = strtod(token.start, nullptr);
        int ki = state_->emitter.add_constant(val_float(val));
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    /* =========================================================
    ** String literal
    ** ========================================================= */

    int Compiler::string_literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Strip quotes (single or triple) */
        const char *str = token.start + 1;
        int len = token.length - 2;

        /* Check for triple quotes */
        if (token.length >= 6 &&
            token.start[0] == token.start[1] && token.start[1] == token.start[2])
        {
            str = token.start + 3;
            len = token.length - 6;
        }

        int ki = state_->emitter.add_escaped_string_constant(str, len);
        if (state_->emitter.has_escape_error())
        {
            error(state_->emitter.escape_error());
            state_->emitter.clear_escape_error();
        }
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    /* =========================================================
    ** F-string: f"hello {expr} world"
    ** Compiles to: str_part + str(expr) + str_part + ...
    ** ========================================================= */

    int Compiler::fstring_literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Content is between f" and " — strip f" prefix and closing " */
        const char *raw = token.start + 2; /* skip f" */
        int raw_len = token.length - 3;    /* skip f" and closing " */
        /* Handle triple-quoted f-strings: f"""...""" */
        if (token.length >= 8 && token.start[2] == token.start[3] && token.start[3] == token.start[4])
        {
            raw = token.start + 5;
            raw_len = token.length - 8;
        }

        /* Accumulate literal chars to emit as string pieces */
        char buf[4096];
        int buf_len = 0;
        bool has_result = false;
        int line = token.line;

        /* Helper: emit current buf as string constant and ADD to reg */
        auto flush_literal = [&]()
        {
            if (buf_len == 0)
                return;
            int ki = state_->emitter.add_escaped_string_constant(buf, buf_len);
            int tmp = alloc_reg();
            state_->emitter.emit_abx(OP_LOADK, tmp, ki, line);
            if (!has_result)
            {
                emit_move(reg, tmp);
                has_result = true;
            }
            else
            {
                state_->emitter.emit_abc(OP_ADD, reg, reg, tmp, line);
            }
            free_reg(tmp);
            buf_len = 0;
        };

        const char *p = raw;
        const char *end = raw + raw_len;
        while (p < end)
        {
            if (*p == '{' && p + 1 < end && *(p + 1) == '{')
            {
                buf[buf_len++] = '{';
                p += 2;
                continue;
            }
            if (*p == '}' && p + 1 < end && *(p + 1) == '}')
            {
                buf[buf_len++] = '}';
                p += 2;
                continue;
            }
            if (*p == '{')
            {
                /* Start of expression */
                flush_literal();
                p++; /* skip { */
                /* Find matching } (no nested braces for now) */
                const char *expr_start = p;
                while (p < end && *p != '}')
                    p++;
                if (p >= end)
                {
                    error("Unterminated '{' in f-string.");
                    break;
                }
                int expr_len = (int)(p - expr_start);
                p++; /* skip } */

                /* Compile the expression by feeding it to a sub-lexer */
                char expr_buf[1024];
                if (expr_len >= (int)sizeof(expr_buf) - 4)
                {
                    error("F-string expression too long.");
                    break;
                }
                memcpy(expr_buf, expr_start, expr_len);
                expr_buf[expr_len] = '\n';
                expr_buf[expr_len + 1] = '\0';

                /* Save lexer + token state, init sub-lexer for expression */
                LexerState saved_lex = lexer_.save_state();
                Token saved_prev = previous_;
                Token saved_curr = current_;

                lexer_.init(expr_buf, current_file_);
                advance(); /* prime current_ */

                int expr_reg = expression(-1);

                /* Restore lexer */
                lexer_.restore_state(saved_lex);
                previous_ = saved_prev;
                current_ = saved_curr;

                /* Emit: str(expr_reg) */
                int str_gidx = find_or_add_global("str", 3);
                int str_reg = alloc_reg();
                int arg_reg = alloc_reg();
                state_->emitter.emit_abx(OP_GETGLOBAL, str_reg, str_gidx, line);
                emit_move(arg_reg, expr_reg);
                state_->emitter.emit_abc(OP_CALL, str_reg, 1, 1, line);
                free_reg(arg_reg);
                /* result in str_reg */

                if (!has_result)
                {
                    emit_move(reg, str_reg);
                    has_result = true;
                }
                else
                {
                    state_->emitter.emit_abc(OP_ADD, reg, reg, str_reg, line);
                }
                free_reg(str_reg);
                if (expr_reg != reg && expr_reg != str_reg)
                    free_reg(expr_reg);
                continue;
            }
            /* Regular char — handle escape sequences for the buffer */
            if (*p == '\\' && p + 1 < end)
            {
                p++;
                switch (*p)
                {
                case 'n':
                    buf[buf_len++] = '\n';
                    break;
                case 't':
                    buf[buf_len++] = '\t';
                    break;
                case 'r':
                    buf[buf_len++] = '\r';
                    break;
                case '\\':
                    buf[buf_len++] = '\\';
                    break;
                case '"':
                    buf[buf_len++] = '"';
                    break;
                case '\'':
                    buf[buf_len++] = '\'';
                    break;
                default:
                    buf[buf_len++] = '\\';
                    buf[buf_len++] = *p;
                    break;
                }
                p++;
            }
            else
            {
                buf[buf_len++] = *p++;
            }
        }
        flush_literal();

        /* If no content at all, emit empty string */
        if (!has_result)
        {
            int ki = state_->emitter.add_escaped_string_constant("", 0);
            state_->emitter.emit_abx(OP_LOADK, reg, ki, line);
        }

        return reg;
    }

    /* =========================================================
    ** Literals: True, False, None
    ** ========================================================= */

    int Compiler::literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        switch (token.type)
        {
        case TOK_TRUE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 1, 0, token.line);
            break;
        case TOK_FALSE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 0, 0, token.line);
            break;
        case TOK_NONE:
            state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, token.line);
            break;
        default:
            break;
        }
        return reg;
    }

    /* =========================================================
    ** Variable access (identifier)
    ** ========================================================= */

    int Compiler::variable(Token token, int dest, bool can_assign)
    {
        /* Intercept eval("...") as OP_EVAL intrinsic */
        if (token.length == 4 && memcmp(token.start, "eval", 4) == 0 && check(TOK_LPAREN))
        {
            advance(); /* consume '(' */
            int reg = (dest >= 0) ? dest : alloc_reg();
            /* Allocate arg slot one above result */
            int arg_reg = reg + 1;
            while (state_->next_reg <= arg_reg)
                alloc_reg();
            expression(arg_reg);
            consume(TOK_RPAREN, "Expected ')' after eval argument.");
            state_->emitter.emit_abc(OP_EVAL, reg, arg_reg, 0, token.line);
            state_->next_reg = reg + 1;
            return reg;
        }
        return named_variable(token, dest, can_assign);
    }

    /* =========================================================
    ** Unary operators: -x, ~x, not x
    ** ========================================================= */

    int Compiler::unary(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        int operand = parse_precedence(PREC_UNARY, -1);

        switch (token.type)
        {
        case TOK_MINUS:
            state_->emitter.emit_abc(OP_NEG, reg, operand, 0, token.line);
            break;
        case TOK_TILDE:
            state_->emitter.emit_abc(OP_BNOT, reg, operand, 0, token.line);
            break;
        case TOK_NOT:
            state_->emitter.emit_abc(OP_NOT, reg, operand, 0, token.line);
            break;
        default:
            break;
        }
        if (operand != reg)
            free_reg(operand);
        return reg;
    }

    /* =========================================================
    ** Binary operators
    ** ========================================================= */

    int Compiler::binary(Token op, int left, int dest)
    {
        int prec = get_precedence(op.type);
        int adjust = is_right_associative(op.type) ? 0 : 1;
        int right_start = state_->emitter.current_offset();
        int right = parse_precedence(prec + adjust, -1);

        /* Peephole: `x + <small int literal>` / `x - <small int literal>`
        ** (the `i + 1` of every loop counter). When the right operand
        ** compiled to exactly one instruction and it is a LOADI into the
        ** operand's own fresh temporary with an immediate that fits the
        ** 8-bit C field, drop that LOADI and emit the immediate form
        ** instead — one instruction and one register fewer. OP_ADDI/OP_SUBI
        ** are defined to behave exactly like OP_ADD/OP_SUB with that int
        ** right operand for every left-operand type (instances with
        ** __add__, strings, ...), so this is purely an encoding change. */
        if ((op.type == TOK_PLUS || op.type == TOK_MINUS) &&
            state_->emitter.current_offset() == right_start + 1)
        {
            Instruction li = state_->emitter.instruction_at(right_start);
            if (ZEN_OP(li) == OP_LOADI && ZEN_A(li) == right)
            {
                int imm = ZEN_SBX(li);
                if (imm >= -128 && imm <= 127)
                {
                    state_->emitter.shrink_to(right_start);
                    free_reg(right); /* the literal's temporary no longer exists */
                    int ireg = (dest >= 0) ? dest : alloc_reg();
                    state_->emitter.emit_abc(op.type == TOK_PLUS ? OP_ADDI : OP_SUBI,
                                             ireg, left, (uint8_t)(int8_t)imm, op.line);
                    if (left != ireg)
                        free_reg(left);
                    return ireg;
                }
            }
        }

        int reg = (dest >= 0) ? dest : alloc_reg();

        OpCode opcode;
        switch (op.type)
        {
        case TOK_PLUS:
            opcode = OP_ADD;
            break;
        case TOK_MINUS:
            opcode = OP_SUB;
            break;
        case TOK_STAR:
            opcode = OP_MUL;
            break;
        case TOK_SLASH:
            opcode = OP_DIV;
            break;
        case TOK_PERCENT:
            opcode = OP_MOD;
            break;
        case TOK_DSLASH:
            opcode = OP_IDIV;
            break;
        case TOK_DSTAR:
            opcode = OP_POW;
            break;
        case TOK_AMP:
            opcode = OP_BAND;
            break;
        case TOK_PIPE:
            opcode = OP_BOR;
            break;
        case TOK_CARET:
            opcode = OP_BXOR;
            break;
        case TOK_LSHIFT:
            opcode = OP_SHL;
            break;
        case TOK_RSHIFT:
            opcode = OP_SHR;
            break;
        default:
            opcode = OP_ADD;
            break;
        }

        state_->emitter.emit_abc(opcode, reg, left, right, op.line);

        /* `self.field * local` is the innermost operation in the usual game
        ** update (`self.x + self.vx * dt`).  With a local right operand its
        ** bytecode is adjacent GETFIELD_IDX / MUL, so fuse their dispatches.
        ** OP_GETFIELD_MUL keeps the original MUL word and deopts to it for
        ** object/string operands: this changes encoding, not semantics. */
        if (opcode == OP_MUL &&
            state_->emitter.current_offset() == right_start + 1 &&
            right_start > 0)
        {
            Instruction field_load = state_->emitter.instruction_at(right_start - 1);
            if (ZEN_OP(field_load) == OP_GETFIELD_IDX && ZEN_A(field_load) == left)
                state_->emitter.rewrite_opcode_at(right_start - 1, OP_GETFIELD_MUL);
        }
        if (right != reg)
            free_reg(right);
        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Comparison operators
    ** ========================================================= */

    int Compiler::comparison(Token op, int left, int dest)
    {
        /* 'not in' — consume 'in' first, then parse RHS */
        if (op.type == TOK_NOT)
        {
            if (!match(TOK_IN))
            {
                error("Expected 'in' after 'not' in comparison.");
                return left;
            }
            int right = parse_precedence(PREC_COMPARISON + 1, -1);
            int reg = (dest >= 0) ? dest : alloc_reg();
            state_->emitter.emit_abc(OP_CONTAINS, reg, left, right, op.line);
            state_->emitter.emit_abc(OP_NOT, reg, reg, 0, op.line);
            if (right != reg)
                free_reg(right);
            if (left != reg)
                free_reg(left);
            return reg;
        }

        int right = parse_precedence(get_precedence(op.type) + 1, -1);
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Helper: emit a single comparison op into reg */
        auto emit_cmp = [&](TokenType t, int lhs, int rhs)
        {
            switch (t)
            {
            case TOK_EQEQ:
                state_->emitter.emit_abc(OP_EQ, reg, lhs, rhs, op.line);
                break;
            case TOK_BANGEQ:
                state_->emitter.emit_abc(OP_EQ, reg, lhs, rhs, op.line);
                state_->emitter.emit_abc(OP_NOT, reg, reg, 0, op.line);
                break;
            case TOK_LT:
                state_->emitter.emit_abc(OP_LT, reg, lhs, rhs, op.line);
                break;
            case TOK_GT:
                state_->emitter.emit_abc(OP_LT, reg, rhs, lhs, op.line);
                break;
            case TOK_LTEQ:
                state_->emitter.emit_abc(OP_LE, reg, lhs, rhs, op.line);
                break;
            case TOK_GTEQ:
                state_->emitter.emit_abc(OP_LE, reg, rhs, lhs, op.line);
                break;
            case TOK_IS:
                state_->emitter.emit_abc(OP_IS, reg, lhs, rhs, op.line);
                break;
            case TOK_IN:
                state_->emitter.emit_abc(OP_CONTAINS, reg, lhs, rhs, op.line);
                break;
            default:
                break;
            }
        };

        emit_cmp(op.type, left, right);
        if (left != reg && left != right)
            free_reg(left);

        /* Chained comparisons: 0 < x < 10 → (0 < x) and (x < 10) */
        static const TokenType chain_ops[] = {
            TOK_LT, TOK_GT, TOK_LTEQ, TOK_GTEQ, TOK_EQEQ, TOK_BANGEQ};
        while (true)
        {
            TokenType next = current_.type;
            bool is_chain = false;
            for (auto ct : chain_ops)
                if (next == ct)
                {
                    is_chain = true;
                    break;
                }
            if (!is_chain)
                break;

            /* Short-circuit: if first comparison false, skip rest */
            int and_jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, previous_.line);

            /* Advance past the operator */
            Token chain_op = current_;
            advance();

            /* Parse next operand; keep right alive as new left */
            int new_right = parse_precedence(PREC_COMPARISON + 1, -1);
            emit_cmp(chain_op.type, right, new_right);

            /* Free old right, new left for next iteration */
            if (right != reg)
                free_reg(right);
            right = new_right;

            state_->emitter.patch_jump(and_jump);
        }

        if (right != reg)
            free_reg(right);
        return reg;
    }

    /* =========================================================
    ** Logical and/or (short-circuit)
    ** ========================================================= */

    int Compiler::logical_and(int left, int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
        }
        else
        {
            bool left_is_local = false;
            for (int i = 0; i < state_->local_count; i++)
                if (state_->locals[i].reg == left)
                {
                    left_is_local = true;
                    break;
                }
            reg = left_is_local ? alloc_reg() : left;
        }
        if (left != reg)
            emit_move(reg, left);

        /* If falsy, jump over RHS (short-circuit: keep LHS value) */
        int jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, previous_.line);

        /* Free left only if it's a different temporary from reg */
        if (left != reg)
            free_reg(left);

        /* Ensure next_reg is above reg so RHS sub-expressions don't overwrite it */
        int saved_next = state_->next_reg;
        if (state_->next_reg <= reg)
            state_->next_reg = reg + 1;

        /* dest=-1, not reg: passing reg here forces named_variable() to
        ** MOVE any bare name read on the RHS (most importantly `self`)
        ** into reg immediately, before a following `.field` gets a chance
        ** to see it — is_current_class_instance() only recognizes literal
        ** register 0 as "this is self", so a copy silently downgrades
        ** `self.field` from OP_GETFIELD_IDX (O(1)) to OP_GETFIELD's
        ** by-name lookup for the rest of the RHS. With dest=-1, a bare
        ** `self`/local on the RHS is returned in its own original
        ** register untouched, and the single MOVE this function already
        ** performs below still happens — just after the RHS finishes
        ** instead of before dot_expr() gets to look at it. */
        int right = parse_precedence(PREC_AND + 1, -1);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->next_reg = saved_next > state_->next_reg ? saved_next : state_->next_reg;
        state_->emitter.patch_jump(jump);
        return reg;
    }

    /* =========================================================
    ** Ternary: true_val if condition else false_val
    ** `left` already holds true_val (evaluated before `if` token)
    ** ========================================================= */

    int Compiler::ternary_expr(int true_val, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Move true_val into result reg */
        if (true_val != reg)
            emit_move(reg, true_val);

        /* condition */
        int cond = parse_precedence(PREC_OR, -1);

        /* JMPIFNOT: if cond is false, jump to else branch */
        int jump_to_else = state_->emitter.emit_jump(OP_JMPIFNOT, cond, previous_.line);
        free_reg(cond);

        /* Jump over else branch (taken when condition is true) */
        int jump_over_else = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);

        /* Patch jump_to_else here (else branch) */
        state_->emitter.patch_jump(jump_to_else);

        consume(TOK_ELSE, "Expected 'else' in ternary expression.");

        /* dest=-1, not reg — same reasoning as logical_and()/logical_or():
        ** avoids forcing a premature MOVE of a bare self/local before a
        ** following `.field` can use OP_GETFIELD_IDX's O(1) path. */
        int false_val = parse_precedence(PREC_TERNARY + 1, -1);
        if (false_val != reg)
            emit_move(reg, false_val);

        /* Patch jump over else */
        state_->emitter.patch_jump(jump_over_else);

        if (true_val != reg)
            free_reg(true_val);
        if (false_val != reg)
            free_reg(false_val);
        return reg;
    }

    int Compiler::logical_or(int left, int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
        }
        else
        {
            bool left_is_local = false;
            for (int i = 0; i < state_->local_count; i++)
                if (state_->locals[i].reg == left)
                {
                    left_is_local = true;
                    break;
                }
            reg = left_is_local ? alloc_reg() : left;
        }
        if (left != reg)
            emit_move(reg, left);

        /* If truthy, jump over RHS (short-circuit: keep LHS value) */
        int jump = state_->emitter.emit_jump(OP_JMPIF, reg, previous_.line);

        /* Free left only if it's a different temporary from reg */
        if (left != reg)
            free_reg(left);

        /* Ensure next_reg is above reg so RHS sub-expressions don't overwrite it */
        int saved_next = state_->next_reg;
        if (state_->next_reg <= reg)
            state_->next_reg = reg + 1;

        /* dest=-1, not reg — same reasoning as logical_and() above. */
        int right = parse_precedence(PREC_OR + 1, -1);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->next_reg = saved_next > state_->next_reg ? saved_next : state_->next_reg;
        state_->emitter.patch_jump(jump);
        return reg;
    }

    /* True when the token immediately following (current_, already
    ** advanced past everything this call/dot/subscript just consumed)
    ** continues the same postfix chain at PREC_CALL — `.`, `(`, `[`, `?.`.
    ** Used to adjourn a call's own move-into-dest: if another link is
    ** coming right after, THAT link will receive this result as its
    ** `callee`/`obj` and do the eventual move into `dest` once it really
    ** is the last one — collapsing what would otherwise be a MOVE-out
    ** here immediately followed by a MOVE-in there. Safe because a
    ** postfix chain never lets anything else observe the intermediate
    ** register by name in between: each link hands its result straight to
    ** the next as an explicit operand, instruction after instruction. */
    bool Compiler::chain_continues() const
    {
        return current_.type == TOK_DOT || current_.type == TOK_LPAREN ||
               current_.type == TOK_LBRACKET || current_.type == TOK_QDOT;
    }

    /* =========================================================
    ** Function call: callee(args...)
    ** ========================================================= */

    int Compiler::call_expr(int callee, int dest)
    {
        /* Resolve the signature before parsing arguments: nested calls in the
        ** argument list overwrite pending_callee_. */
        const FuncSig *sig = callee_signature();

        /* OP_CALL overwrites R[base] with the result, so a LOCAL's own
        ** register can't serve as base. But the callee is usually not a
        ** local: it's the temporary a GETGLOBAL/GETFIELD/previous call just
        ** produced at the top of the register stack — already exactly where
        ** base needs to be. (The previous `base < next_reg` test here was
        ** true for every live register, so every call paid a MOVE.) Same
        ** rule as dot_expr()'s receiver reuse: not a local AND top of stack. */
        bool callee_is_local = false;
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].reg == callee)
            {
                callee_is_local = true;
                break;
            }
        }
        bool callee_is_top = (callee == state_->next_reg - 1);
        int base = (!callee_is_local && callee_is_top) ? callee : alloc_reg();
        if (base != callee)
            emit_move(base, callee);

        /* A direct call to a global name — `fib(n)`, `helper(x)`, any
        ** module-level def or imported function — arrives here as
        ** `GETGLOBAL base` immediately followed by this call. Fold the pair
        ** into OP_CALLGLOBAL: the VM reads the global into R[base] itself
        ** and continues on OP_CALL's path, one dispatch fewer per call. */
        int fused_global_idx = -1;
        if (base == callee && state_->emitter.current_offset() > 0)
        {
            int prev_off = state_->emitter.current_offset() - 1;
            Instruction prev = state_->emitter.instruction_at(prev_off);
            if (ZEN_OP(prev) == OP_GETGLOBAL && (int)ZEN_A(prev) == callee)
            {
                fused_global_idx = (int)ZEN_BX(prev);
                state_->emitter.shrink_to(prev_off);
            }
        }

        /* Parse arguments into consecutive registers after callee */
        int nargs = argument_list(base, 0, sig);

        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* OP_CALL: R[base](R[base+1]..R[base+nargs]) → R[base] */
        if (fused_global_idx >= 0)
            state_->emitter.emit_callglobal(base, nargs, 1, fused_global_idx, previous_.line);
        else
            state_->emitter.emit_abc(OP_CALL, base, nargs, 1, previous_.line);

        /* Restore registers: call result is in base */
        state_->next_reg = base + 1;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;

        int result = base;
        /* If another link continues the chain right after this call
        ** (`f()()`, `f().x`, `f()[0]`), defer the move into dest — that
        ** next link will receive `result` as its own callee/obj and do the
        ** eventual move once IT turns out to be the chain's last link. */
        if (dest >= 0 && dest != result && !chain_continues())
        {
            emit_move(dest, result);
            free_reg(result);
            return dest;
        }
        return result;
    }

    int Compiler::generic_call_expr(int callee, int dest)
    {
        const FuncSig *sig = callee_signature();

        /* OP_CALL_GENERIC overwrites R[base] with the return value, just
        ** like a normal call — same base-reuse rule as call_expr(). */
        bool callee_is_local = false;
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].reg == callee)
            {
                callee_is_local = true;
                break;
            }
        }
        bool callee_is_top = (callee == state_->next_reg - 1);
        int base = (!callee_is_local && callee_is_top) ? callee : alloc_reg();
        if (base != callee)
            emit_move(base, callee);

        int ngeneric = 0;
        int nargs = generic_argument_list(base, sig, &ngeneric);
        if (nargs & 0x80)
        {
            error("Cannot spread arguments into a generic call.");
            nargs &= 0x7F;
        }
        state_->emitter.emit_abc(OP_CALL_GENERIC, base, nargs, 1, previous_.line);
        state_->emitter.emit((uint32_t)(ngeneric & 0xFFFF), previous_.line);

        state_->next_reg = base + 1;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;

        if (dest >= 0 && dest != base)
        {
            emit_move(dest, base);
            free_reg(base);
            return dest;
        }
        return base;
    }

    /* True when the next two tokens are `name =` — a keyword argument.
    ** `==` is a different token, so a comparison never looks like one. */
    bool Compiler::next_is_keyword_arg()
    {
        if (current_.type != TOK_IDENTIFIER)
            return false;
        LexerState saved = lexer_.save_state();
        bool eq = lexer_.next_token().type == TOK_EQ;
        lexer_.restore_state(saved);
        return eq;
    }

    int Compiler::argument_list(int base, int initial_nargs, const FuncSig *sig)
    {
        int nargs = initial_nargs;
        bool has_spread = false;

        /* Keyword arguments are resolved here and vanish: each one is
        ** compiled straight into the register its parameter occupies, and
        ** any parameter skipped along the way gets its declared default
        ** emitted in place.  What OP_CALL sees is an ordinary positional
        ** call. */
        uint64_t filled = 0;
        int highest = -1;
        bool saw_keyword = false;

        if (!check(TOK_RPAREN))
        {
            do
            {
                if (next_is_keyword_arg())
                {
                    /* `name = value` inside a call is a keyword argument, never
                    ** an assignment expression.  Saying so out loud beats
                    ** compiling it as one and passing the wrong positional. */
                    if (!sig)
                    {
                        error("Keyword argument needs a signature the compiler "
                              "can see (a def or method in this file).");
                        return nargs;
                    }
                    if (has_spread)
                    {
                        error("Keyword argument cannot follow '*' spread.");
                        return nargs;
                    }
                    if (!sig->takes_keywords)
                    {
                        error("This function does not accept keyword arguments.");
                        return nargs;
                    }
                    advance(); /* the parameter name */
                    Token key = previous_;
                    advance(); /* '=' */

                    int idx = sig_param_index(sig, key);
                    if (idx < 0)
                    {
                        error("Unknown parameter name in keyword argument.");
                        return nargs;
                    }
                    if (idx < nargs || (filled & (1ull << idx)))
                    {
                        error("Parameter already given a value.");
                        return nargs;
                    }

                    int arg_reg = base + 1 + idx;
                    if (arg_reg >= kMaxRegisters)
                    {
                        error("Too many arguments.");
                        return nargs;
                    }
                    while (state_->next_reg <= arg_reg)
                        alloc_reg();

                    int r = expression(arg_reg);
                    if (r != arg_reg)
                        emit_move(arg_reg, r);
                    filled |= 1ull << idx;
                    if (idx > highest)
                        highest = idx;
                    saw_keyword = true;
                    continue;
                }

                if (saw_keyword)
                {
                    error("Positional argument cannot follow a keyword argument.");
                    return nargs;
                }

                int arg_reg = base + 1 + nargs;
                if (arg_reg >= kMaxRegisters)
                {
                    error("Too many arguments.");
                    return nargs;
                }

                if (match(TOK_STAR))
                {
                    /* *expr — spread: must be last arg */
                    while (state_->next_reg <= arg_reg)
                        alloc_reg();
                    has_spread = true;
                    expression(arg_reg);
                    nargs++;
                    break; /* no more args after spread */
                }

                /* Compile the argument with dest=-1 rather than dest=arg_reg.
                ** arg_reg is the top of the register stack here (base and
                ** the previous arguments sit right below it, nothing live
                ** above), so an expression that allocates its result simply
                ** lands on arg_reg by itself — and one that doesn't (a bare
                ** local, `self`) is copied there afterwards, exactly the MOVE
                ** dest=arg_reg would have emitted anyway. What changes is
                ** WHEN that copy happens: with dest=arg_reg, named_variable()
                ** moved `self` into arg_reg before a following `.field` was
                ** even parsed, and dot_expr()'s self-fast-path only recognizes
                ** register 0, so `f(self.x)` compiled to MOVE + a by-name
                ** OP_GETFIELD instead of a single OP_GETFIELD_IDX. Keyword
                ** arguments keep dest=arg_reg: they can land above other
                ** live arguments, where "nothing above is live" doesn't hold. */
                while (state_->next_reg < arg_reg)
                    alloc_reg();
                int r = expression(-1);
                if (r != arg_reg)
                {
                    while (state_->next_reg <= arg_reg)
                        alloc_reg();
                    emit_move(arg_reg, r);
                }
                /* The value is in arg_reg; any temporaries the expression
                ** left above it are dead. */
                state_->next_reg = arg_reg + 1;
                nargs++;
            } while (match(TOK_COMMA));
        }

        if (saw_keyword)
        {
            /* Fill the gaps the keywords jumped over.  Parameters past the
            ** last one named are left to the VM, which already tops a call
            ** up from ObjFunc::defaults. */
            for (int i = nargs; i <= highest; i++)
            {
                if (filled & (1ull << i))
                    continue;
                const SigParam &p = sig_params_[sig->param_start + i];
                if (!p.has_default)
                {
                    error("Skipped parameter has no default value.");
                    return nargs;
                }
                int arg_reg = base + 1 + i;
                while (state_->next_reg <= arg_reg)
                    alloc_reg();
                emit_sig_default(p, arg_reg);
            }
            nargs = highest + 1;
        }

        /* Encode spread flag in bit 7 of nargs */
        if (has_spread)
            nargs |= 0x80;
        return nargs;
    }

    /* Parse <T, U>(args) after a callee.  Type arguments are normal runtime
    ** values (classes, checked against at runtime by OP_CALL_GENERIC /
    ** OP_INVOKE_GENERIC) placed before the explicit value arguments — but,
    ** unlike the old f<T>(x) == f(T,x) sugar, they are counted separately
    ** (ObjFunc::generic_arity) rather than folded into the same arity as
    ** the value parameters. */
    int Compiler::generic_argument_list(int base, const FuncSig *sig, int *out_ngeneric)
    {
        consume(TOK_LT, "Expected '<' before generic arguments.");

        int ngeneric = 0;
        do
        {
            consume(TOK_IDENTIFIER, "Expected generic type name.");
            Token type_name = previous_;

            int arg_reg = base + 1 + ngeneric;
            if (arg_reg >= kMaxRegisters)
            {
                error("Too many generic arguments.");
                *out_ngeneric = ngeneric;
                return ngeneric;
            }
            while (state_->next_reg <= arg_reg)
                alloc_reg();

            int type_reg = named_variable(type_name, arg_reg, false);
            if (type_reg != arg_reg)
                emit_move(arg_reg, type_reg);
            ngeneric++;
        } while (match(TOK_COMMA));

        consume(TOK_GT, "Expected '>' after generic arguments.");

        /* Compile-time check when the callee's signature is visible: catches
        ** `def f<T>(x)` called as `f<T,U>()`, and (via sig->generic_count==0)
        ** a non-generic function called with `<...>` — both used to silently
        ** compile as extra/misplaced positional arguments. */
        if (sig && sig->generic_count != ngeneric)
        {
            if (sig->generic_count == 0)
                error("Function is not generic.");
            else
                error("Wrong number of type arguments for generic function.");
        }

        consume(TOK_LPAREN, "Expected '(' after generic arguments.");

        int nvalue = argument_list(base + ngeneric, 0, sig);
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        *out_ngeneric = ngeneric;
        /* nvalue may carry the spread flag in bit 7 (see argument_list) —
        ** preserve it untouched, only the low 7 bits are an actual count.
        ** ngeneric + (nvalue's count) must itself stay under 128: past
        ** that, the sum spills into bit 7 and gets misread as a spread
        ** flag by every caller's `if (nargs & 0x80)` check — reject before
        ** that arithmetic can produce a bogus count. kMaxRegisters (250)
        ** bounds each half individually but not their sum, so this can't
        ** be caught earlier by the per-register checks alone. */
        int total = ngeneric + (nvalue & 0x7F);
        if (total > 0x7F)
        {
            error("Too many combined type and value arguments in a generic call.");
            total &= 0x7F;
        }
        return total | (nvalue & 0x80);
    }

    /* Two tokens are "adjacent" when nothing (not even a space) separates
    ** them in the source — comparing the raw pointers avoids needing a
    ** whitespace-aware lexer mode just for this. */
    static inline bool tokens_adjacent(const Token &a, const Token &b)
    {
        return b.start == a.start + a.length;
    }

    /* `f<T>(...)` is generic call syntax ONLY when (a) `f` is already known
    ** to be generic — a script def/method (FuncSig) or a native class
    ** method (ObjNative::generic_arity > 0); `callee_is_generic=false` means
    ** "no, it's a plain variable/expression" and this returns false without
    ** even looking at the tokens — and (b) its complete shape is punctuation
    ** that cannot also be a comparison: the opening '<' glued to the callee
    ** and directly followed by an identifier with no space (`f<T` yes,
    ** `f < T` no), and the closing '>' directly followed by '(' with no
    ** space (`>(` yes, `> (` no). Both conditions are needed: adjacency
    ** alone still reads `f<T, U>(h)` as generic syntax when `f` merely holds
    ** an int — indistinguishable from `f < T, U > (h)` written without
    ** spaces, two chained comparisons in a parenthesized tuple. Ordinary
    ** whitespace is still allowed *inside* the type-argument list
    ** (`f<T, U>(...)`, comma-space is normal style) — only the two boundary
    ** tokens that actually collide with comparison syntax are held to
    ** strict adjacency. */
    bool Compiler::generic_call_ahead(bool callee_is_generic)
    {
        if (!callee_is_generic)
            return false;
        if (!check(TOK_LT))
            return false;
        Token lt = current_;

        LexerState saved = lexer_.save_state();
        Token token = lexer_.next_token();
        if (token.type != TOK_IDENTIFIER || !tokens_adjacent(lt, token))
        {
            lexer_.restore_state(saved);
            return false;
        }

        for (;;)
        {
            token = lexer_.next_token();
            if (token.type != TOK_COMMA)
                break;
            token = lexer_.next_token();
            if (token.type != TOK_IDENTIFIER)
            {
                lexer_.restore_state(saved);
                return false;
            }
        }

        bool is_generic_call = false;
        if (token.type == TOK_GT)
        {
            Token gt = token;
            Token lparen = lexer_.next_token();
            is_generic_call = lparen.type == TOK_LPAREN && tokens_adjacent(gt, lparen);
        }
        lexer_.restore_state(saved);
        return is_generic_call;
    }

    /* =========================================================
    ** Dot access: obj.field or obj.method(args)
    ** ========================================================= */

    int Compiler::dot_expr(int obj, int dest, bool can_assign)
    {
        consume(TOK_IDENTIFIER, "Expected field name after '.'.");
        Token field = previous_;

        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Resolved once, reused by both generic_call_ahead() checks below —
        ** a null sig (method name not seen by the pre-scan, or `obj` isn't
        ** provably an instance of a known class) means `<` after `field`
        ** can never be read as a generic call, only as a comparison —
        ** UNLESS the receiver's static class is a native class with this
        ** name registered as a generic method (ClassBuilder::generic_method,
        ** which script pre-scan knows nothing about). */
        const FuncSig *method_sig = method_signature(obj, field);
        int native_generic_arity = 0;
        bool is_native_generic = false;
        const char *receiver_class_name = nullptr;
        int32_t receiver_class_len = 0;
        const bool receiver_has_static_class =
            receiver_static_class(obj, receiver_class_name, receiver_class_len);
        const bool receiver_is_typed_subscript = (obj == typed_subscript_reg_);
        if (receiver_is_typed_subscript)
            typed_subscript_reg_ = -1; /* type belongs to this one dot only */
        if (!method_sig)
        {
            if (receiver_has_static_class)
                is_native_generic = native_generic_method_arity(receiver_class_name, receiver_class_len,
                                                                  field, native_generic_arity);
        }
        bool method_is_generic = method_sig || is_native_generic;

        /* --- Fast path: self.field inside a method → OP_GETFIELD_IDX / OP_SETFIELD_IDX --- */
        if (is_current_class_instance(obj))
        {
            ObjString *fname = token_string(field);
            int fidx = add_class_field(fname);
            if (fidx >= 0 && fidx <= 255)
            {
                /* Assignment: self.field = expr */
                if (can_assign && match(TOK_EQ))
                {
                    int rhs_start = state_->emitter.current_offset();
                    int val = expression(-1);
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj, fidx, val, field.line);

                    /* Fuse the exact bytecode shape produced by
                    **   self.x = self.x + self.vx * dt
                    ** into one numeric hot path. Its four following words
                    ** are deliberately retained: if a field becomes a
                    ** string or an overloaded object, the VM runs those
                    ** ordinary instructions unchanged. */
                    int rhs_end = state_->emitter.current_offset();
                    if (rhs_end == rhs_start + 5)
                    {
                        Instruction load_x = state_->emitter.instruction_at(rhs_start);
                        Instruction load_v = state_->emitter.instruction_at(rhs_start + 1);
                        Instruction mul = state_->emitter.instruction_at(rhs_start + 2);
                        Instruction add = state_->emitter.instruction_at(rhs_start + 3);
                        Instruction store_x = state_->emitter.instruction_at(rhs_start + 4);
                        if (ZEN_OP(load_x) == OP_GETFIELD_IDX &&
                            ZEN_OP(load_v) == OP_GETFIELD_MUL &&
                            ZEN_OP(mul) == OP_MUL &&
                            ZEN_OP(add) == OP_ADD &&
                            ZEN_OP(store_x) == OP_SETFIELD_IDX &&
                            ZEN_B(load_x) == obj &&
                            ZEN_B(load_v) == obj &&
                            ZEN_A(load_x) == ZEN_B(add) &&
                            ZEN_A(load_v) == ZEN_B(mul) &&
                            ZEN_A(mul) == ZEN_C(add) &&
                            ZEN_A(add) == ZEN_C(store_x) &&
                            ZEN_A(store_x) == obj &&
                            ZEN_C(load_x) == fidx &&
                            ZEN_B(store_x) == fidx)
                        {
                            state_->emitter.rewrite_opcode_at(rhs_start, OP_FIELD_MULADD);
                        }
                    }
                    if (val != reg)
                        free_reg(val);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
                /* Augmented assign: self.field += expr  */
                if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                                   check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) || check(TOK_PERCENT_EQ)))
                {
                    Token op = current_;
                    advance();
                    OpCode arith;
                    switch (op.type)
                    {
                    case TOK_PLUS_EQ:
                        arith = OP_ADD;
                        break;
                    case TOK_MINUS_EQ:
                        arith = OP_SUB;
                        break;
                    case TOK_STAR_EQ:
                        arith = OP_MUL;
                        break;
                    case TOK_SLASH_EQ:
                        arith = OP_DIV;
                        break;
                    default:
                        arith = OP_MOD;
                        break;
                    }
                    int tmp = alloc_reg();
                    state_->emitter.emit_abc(OP_GETFIELD_IDX, tmp, obj, fidx, field.line);
                    int rhs = expression(-1);
                    state_->emitter.emit_abc(arith, tmp, tmp, rhs, op.line);
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj, fidx, tmp, op.line);
                    free_reg(rhs);
                    free_reg(tmp);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
                /* Method call: self.method(args) — fall through to normal path */
                if (!check(TOK_LPAREN) && !generic_call_ahead(method_is_generic))
                {
                    /* Read field */
                    state_->emitter.emit_abc(OP_GETFIELD_IDX, reg, obj, fidx, field.line);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
            }
        }

        /* Assignment: obj.field = expr */
        if (can_assign && match(TOK_EQ))
        {
            int val = expression(-1);
            int name_ki = state_->emitter.add_string_constant(field.start, field.length);
            state_->emitter.emit_abc(OP_SETFIELD, obj, name_ki, val, previous_.line);
            free_reg(val);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Method call: obj.method(args) */
        if (check(TOK_LPAREN) || generic_call_ahead(method_is_generic))
        {
            /* We need a contiguous [receiver, arg1, arg2, ...] block, with
            ** arguments landing at base+1, base+2, ... — so reusing obj as
            ** base is only safe when obj is BOTH (a) not a local (the call
            ** result would clobber it) AND (b) the top of the register
            ** stack (anything below the top between obj and next_reg is
            ** still-live state argument placement would overwrite). Case
            ** (b) is exactly the unnamed temporary a previous call/
            ** sub-expression just produced with nothing allocated after it
            ** — the common shape of a chained `x.a().b()` — where reusing
            ** it in place skips both the MOVE in (obj -> base) and,
            ** correspondingly, the MOVE out below. Same trick already used
            ** by logical_and()/logical_or() for their left operand, plus
            ** the top-of-stack check free_reg() itself relies on. */
            /* `reg` was allocated speculatively at the top of dot_expr for
            ** the field-read shape; a method call's result lands in `base`
            ** instead, so that register is dead here. Give it back BEFORE
            ** the top-of-stack test below — otherwise it sits above obj and
            ** makes a receiver that really is the newest temporary (e.g.
            ** `xs[i].m()`, `f().m()` as a statement) look like it isn't,
            ** costing a fresh base + MOVE on every such call. */
            if (dest < 0)
                free_reg(reg);
            bool obj_is_local = false;
            for (int i = 0; i < state_->local_count; i++)
            {
                if (state_->locals[i].reg == obj)
                {
                    obj_is_local = true;
                    break;
                }
            }
            bool obj_is_top = (obj == state_->next_reg - 1);
            int base = (!obj_is_local && obj_is_top) ? obj : alloc_reg();
            if (base != obj)
                emit_move(base, obj);
            const FuncSig *sig = method_sig;
            int sel = vm_->intern_selector(field.start, field.length);
            int name_ki = state_->emitter.add_string_constant(field.start, field.length);

            if (check(TOK_LPAREN))
            {
                advance(); /* consume '(' */
                int nargs = argument_list(base, 0, sig);
                consume(TOK_RPAREN, "Expected ')' after arguments.");

                /* A call whose receiver has a declared class (including
                   `self`) can use its selector slot directly. Classes are
                   closed after their definition, so this is stable; retain
                   OP_INVOKE for arrays, maps and unknown values. */
                /* `method_sig` exists for ordinary methods too: it is also
                ** used to parse keyword arguments. Only a declaration with
                ** actual type parameters needs OP_INVOKE_GENERIC; those are
                ** the calls OP_INVOKE_VT cannot represent. */
                const bool method_has_type_params =
                    (method_sig && method_sig->generic_count > 0) || is_native_generic;
                /* Array[T][i].method(...) can take the fully static opcode
                ** when the scanner knows every value argument is present.
                ** It is intentionally opt-in through Array[T]: a plain
                ** dynamic receiver keeps the checked call semantics. */
                const bool exact_typed_array_call = receiver_is_typed_subscript &&
                    method_sig && method_sig->takes_keywords &&
                    !method_has_type_params && !(nargs & 0x80) &&
                    nargs == method_sig->param_count && sel <= 255;
                if (exact_typed_array_call)
                {
                    state_->emitter.emit_abc(OP_INVOKE_VT_FAST, base, nargs, sel, field.line);
                }
                else if (receiver_has_static_class && !method_has_type_params && sel <= 255)
                {
                    state_->emitter.emit_abc(OP_INVOKE_VT, base, nargs, sel, field.line);
                }
                else
                {
                    state_->emitter.emit_abc(OP_INVOKE, base, nargs, 1, field.line);
                    state_->emitter.emit((uint32_t)((sel << 16) | (name_ki & 0xFFFF)), field.line);
                }
            }
            else
            {
                int ngeneric = 0;
                int nargs = generic_argument_list(base, sig, &ngeneric);
                if (nargs & 0x80)
                {
                    error("Cannot spread arguments into a generic call.");
                    nargs &= 0x7F;
                }
                /* generic_argument_list only cross-checks ngeneric against a
                ** script FuncSig (sig!=null); a native generic method has no
                ** FuncSig, so check its ObjNative::generic_arity here — same
                ** "wrong number of type arguments" error either way. */
                if (is_native_generic && ngeneric != native_generic_arity)
                {
                    error("Wrong number of type arguments for generic function.");
                }

                /* 3-word instruction: OP_INVOKE_GENERIC + name constant + ngeneric */
                state_->emitter.emit_abc(OP_INVOKE_GENERIC, base, nargs, 1, field.line);
                state_->emitter.emit((uint32_t)((sel << 16) | (name_ki & 0xFFFF)), field.line);
                state_->emitter.emit((uint32_t)ngeneric, field.line);
            }

            state_->next_reg = base + 1;
            /* Defer the move into dest when another link continues the
            ** chain right after (`a.b().c()`, `a.b()[0]`, `a.b()(x)`) —
            ** see call_expr()'s identical comment and chain_continues(). */
            if (dest >= 0 && dest != base && !chain_continues())
            {
                emit_move(dest, base);
                free_reg(base);
                return dest;
            }
            return base;
        }

        /* Field read */
        int name_ki = state_->emitter.add_string_constant(field.start, field.length);
        state_->emitter.emit_abc(OP_GETFIELD, reg, obj, name_ki, field.line);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Optional chaining: obj?.field
    **
    ** If obj is nil, result is nil (no error).
    ** Read-only — cannot assign through ?.
    ** ========================================================= */

    int Compiler::safe_dot_expr(int obj, int dest)
    {
        int line = previous_.line;
        int reg = dest >= 0 ? dest : alloc_reg();
        if (obj != reg)
            emit_move(reg, obj);

        /* If reg is nil (falsy), skip field access — result stays nil */
        int jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, line);

        /* Parse field name */
        consume(TOK_IDENTIFIER, "Expected field name after '?.'.");
        Token field = previous_;
        int name_ki = state_->emitter.add_string_constant(field.start, field.length);

        state_->emitter.emit_abc(OP_GETFIELD, reg, reg, name_ki, line);
        state_->emitter.patch_jump(jump);

        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Null coalescing: left ?? right
    **
    ** If left is truthy, result is left. Otherwise, result is right.
    ** ========================================================= */

    int Compiler::null_coalesce(int left, int dest)
    {
        int line = previous_.line;
        int reg = dest >= 0 ? dest : alloc_reg();
        if (left != reg)
            emit_move(reg, left);

        /* If reg is truthy (not nil/false), skip right side */
        int jump = state_->emitter.emit_jump(OP_JMPIF, reg, line);

        /* Parse right operand. dest=-1, not reg — same reasoning as
        ** logical_and()/logical_or(): avoids forcing a premature MOVE of a
        ** bare self/local before a following `.field` can use
        ** OP_GETFIELD_IDX's O(1) path. */
        int right = parse_precedence(PREC_OR + 1, -1);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->emitter.patch_jump(jump);

        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Subscript: obj[index] or obj[index] = val
    ** ========================================================= */

    int Compiler::subscript_expr(int obj, int dest, bool can_assign)
    {
        /* Slice: a[start:stop:step] — starts with ':' (no start) */
        if (check(TOK_COLON))
            return slice_expr(obj, dest, -1);

        int index = expression(-1);

        /* Slice after parsing start expr: a[start:...] */
        if (check(TOK_COLON))
            return slice_expr(obj, dest, index);

        consume(TOK_RBRACKET, "Expected ']' after subscript.");

        const char *element_class_name = nullptr;
        int32_t element_class_len = 0;
        const bool has_typed_element =
            array_element_class(obj, element_class_name, element_class_len);

        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Assignment: obj[idx] = expr */
        if (can_assign && match(TOK_EQ))
        {
            /* Preserve container/index across RHS evaluation.
            ** RHS parsing can allocate/free temporaries and clobber these regs. */
            int obj_hold = alloc_reg();
            int idx_hold = alloc_reg();
            emit_move(obj_hold, obj);
            emit_move(idx_hold, index);

            int val = expression(-1);
            state_->emitter.emit_abc(OP_SETINDEX, obj_hold, idx_hold, val, previous_.line);
            free_reg(val);
            free_reg(idx_hold);
            free_reg(obj_hold);
            free_reg(index);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Augmented assignment: obj[idx] += expr etc. */
        if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                           check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                           check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                           check(TOK_DSTAR_EQ)))
        {
            Token op = current_;
            advance();
            /* Read current value */
            state_->emitter.emit_abc(OP_GETINDEX, reg, obj, index, previous_.line);
            int rhs = expression(-1);
            OpCode arith = OP_ADD;
            switch (op.type)
            {
            case TOK_PLUS_EQ:
                arith = OP_ADD;
                break;
            case TOK_MINUS_EQ:
                arith = OP_SUB;
                break;
            case TOK_STAR_EQ:
                arith = OP_MUL;
                break;
            case TOK_SLASH_EQ:
                arith = OP_DIV;
                break;
            case TOK_PERCENT_EQ:
                arith = OP_MOD;
                break;
            case TOK_DSLASH_EQ:
                arith = OP_IDIV;
                break;
            case TOK_DSTAR_EQ:
                arith = OP_POW;
                break;
            default:
                break;
            }
            state_->emitter.emit_abc(arith, reg, reg, rhs, previous_.line);
            free_reg(rhs);
            state_->emitter.emit_abc(OP_SETINDEX, obj, index, reg, previous_.line);
            free_reg(index);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Read */
        state_->emitter.emit_abc(OP_GETINDEX, reg, obj, index, previous_.line);
        if (has_typed_element)
        {
            typed_subscript_reg_ = reg;
            typed_subscript_class_.start = element_class_name;
            typed_subscript_class_.length = element_class_len;
        }
        else
        {
            typed_subscript_reg_ = -1;
        }
        free_reg(index);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Slice: container[start:stop:step]
    ** start_reg >= 0  → already evaluated start
    ** start_reg == -1 → start omitted (None)
    ** R[base]=start, R[base+1]=stop, R[base+2]=step
    ** ========================================================= */

    int Compiler::slice_expr(int obj, int dest, int start_reg)
    {
        int line = previous_.line;
        int base = alloc_reg(); /* start */

        if (start_reg >= 0)
        {
            if (start_reg != base)
                emit_move(base, start_reg);
            free_reg(start_reg);
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, base, 0, 0, line);
        }

        advance(); /* consume ':' */

        int stop_reg = alloc_reg(); /* must be base+1 */

        /* stop */
        if (check(TOK_RBRACKET) || check(TOK_COLON))
        {
            state_->emitter.emit_abc(OP_LOADNIL, stop_reg, 0, 0, line);
        }
        else
        {
            int v = expression(stop_reg);
            if (v != stop_reg)
                emit_move(stop_reg, v);
        }

        /* step */
        int step_reg = alloc_reg(); /* must be base+2 */
        if (match(TOK_COLON))
        {
            if (check(TOK_RBRACKET))
                state_->emitter.emit_abc(OP_LOADNIL, step_reg, 0, 0, line);
            else
            {
                int v = expression(step_reg);
                if (v != step_reg)
                    emit_move(step_reg, v);
            }
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, step_reg, 0, 0, line);
        }

        consume(TOK_RBRACKET, "Expected ']' after slice.");

        int reg = (dest >= 0) ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_GETSLICE, reg, obj, base, line);

        free_reg(step_reg);
        free_reg(stop_reg);
        free_reg(base);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Array literal: [a, b, c]  or  [expr for var in iterable [if cond]]
    ** ========================================================= */

    int Compiler::array_literal(int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
            /* Ensure next_reg is past dest so element expressions don't reuse it */
            if (state_->next_reg <= dest)
                state_->next_reg = dest + 1;
        }
        else
        {
            reg = alloc_reg();
        }
        int line = previous_.line;
        state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, line);

        if (!check(TOK_RBRACKET))
        {
            while (match(TOK_NEWLINE))
            {
            }
            if (!check(TOK_RBRACKET))
            {
                /* Save state before parsing first expression — may be a comprehension */
                LexerState lex_save = lexer_.save_state();
                Token cur_save = current_;
                Token prev_save = previous_;
                int off_save = state_->emitter.current_offset();
                int reg_save = state_->next_reg;
                int glob_save = vm_->num_globals();

                int first = expression(-1);

                /* List comprehension: [expr for var in iterable [if cond]] */
                if (check(TOK_FOR))
                {
                    /* Rollback emitted code AND any speculatively-added global slots */
                    state_->emitter.shrink_to(off_save);
                    state_->next_reg = reg_save;
                    vm_->shrink_globals(glob_save);

                    advance(); /* consume 'for' */
                    begin_scope();

                    consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
                    Token var_name = previous_;
                    consume(TOK_IN, "Expected 'in' after variable name.");

                    int iter_reg = alloc_reg();
                    /* Parse iter at PREC_OR so the comprehension 'if' filter token
                       is not treated as a ternary operator. */
                    int iter_result = parse_precedence(PREC_OR, iter_reg);
                    if (iter_result != iter_reg)
                        emit_move(iter_reg, iter_result);

                    int idx_reg = alloc_reg();
                    state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);

                    int len_reg = alloc_reg();
                    state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 1, line);

                    int var_reg = add_local(var_name);

                    int loop_start = state_->emitter.current_offset();

                    int cmp_reg = alloc_reg();
                    state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
                    int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
                    free_reg(cmp_reg);

                    state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

                    /* Optional `if cond` filter */
                    int filter_jump = -1;
                    if (match(TOK_IF))
                    {
                        int cond = expression(-1);
                        filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                        free_reg(cond);
                    }

                    /* Save lex state AFTER filter — this is where we continue after the body */
                    LexerState lex_after_filter = lexer_.save_state();
                    Token cur_after = current_;
                    Token prev_after = previous_;

                    /* Restore lex to before the body expression and re-parse it inside the loop */
                    lexer_.restore_state(lex_save);
                    current_ = cur_save;
                    previous_ = prev_save;
                    (void)first; /* was emitted speculatively — discarded */

                    int body = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, body, 0, line);
                    free_reg(body);

                    /* Restore lex to after the filter (skip re-scanning for/in/iter/if) */
                    lexer_.restore_state(lex_after_filter);
                    current_ = cur_after;
                    previous_ = prev_after;

                    if (filter_jump >= 0)
                        state_->emitter.patch_jump(filter_jump);

                    state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
                    state_->emitter.emit_loop(loop_start, 0, line);
                    state_->emitter.patch_jump(exit_jump);

                    end_scope();
                    /* Restore next_reg to just above the result array reg */
                    state_->next_reg = reg_save;
                    while (match(TOK_NEWLINE))
                    {
                    }
                    consume(TOK_RBRACKET, "Expected ']' after list comprehension.");
                    return reg;
                }

                /* Regular array literal */
                state_->emitter.emit_abc(OP_APPEND, reg, first, 0, line);
                free_reg(first);

                while (match(TOK_COMMA))
                {
                    while (match(TOK_NEWLINE))
                    {
                    }
                    if (check(TOK_RBRACKET))
                        break;
                    int elem = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, elem, 0, previous_.line);
                    free_reg(elem);
                }
            }
        }
        while (match(TOK_NEWLINE))
        {
        }
        consume(TOK_RBRACKET, "Expected ']' after array elements.");
        return reg;
    }

    /* =========================================================
    ** Map/dict literal: {k: v, k: v}
    ** Set literal:      {a, b, c}
    ** Dict/set comprehension: {k: v for x in iter} / {expr for x in iter}
    ** ========================================================= */

    int Compiler::map_literal(int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
            if (state_->next_reg <= dest)
                state_->next_reg = dest + 1;
        }
        else
            reg = alloc_reg();
        int line = previous_.line;

        /* Empty: {} → empty map */
        if (check(TOK_RBRACE))
        {
            advance();
            state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);
            return reg;
        }

        while (match(TOK_NEWLINE))
        {
        }

        /* Save state before first expression to detect set vs map vs comprehension */
        LexerState lex_save = lexer_.save_state();
        Token cur_save = current_;
        Token prev_save = previous_;
        int off_save = state_->emitter.current_offset();
        int reg_save = state_->next_reg;
        int glob_save = vm_->num_globals();

        int first_key = expression(-1);

        /* Dict comprehension: {key: val for var in iter} */
        if (check(TOK_COLON))
        {
            advance();
            state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);

            int first_val = expression(-1);

            if (check(TOK_FOR))
            {
                /* Dict comprehension */
                state_->emitter.shrink_to(off_save);
                state_->next_reg = reg_save;
                vm_->shrink_globals(glob_save);

                /* NEWMAP was rolled back — re-emit it */
                state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);

                advance(); /* consume 'for' */
                begin_scope();
                consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
                Token var_name = previous_;
                consume(TOK_IN, "Expected 'in' after variable name.");

                int iter_reg = alloc_reg();
                int ir = parse_precedence(PREC_OR, iter_reg);
                if (ir != iter_reg)
                    emit_move(iter_reg, ir);
                int idx_reg = alloc_reg();
                state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);
                int len_reg = alloc_reg();
                state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 1, line);
                int var_reg = add_local(var_name);

                int loop_start = state_->emitter.current_offset();
                int cmp_reg = alloc_reg();
                state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
                int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
                free_reg(cmp_reg);
                state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

                int filter_jump = -1;
                if (match(TOK_IF))
                {
                    int cond = expression(-1);
                    filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                    free_reg(cond);
                }

                LexerState lex_after = lexer_.save_state();
                Token cur_after = current_;
                Token prev_after = previous_;

                lexer_.restore_state(lex_save);
                current_ = cur_save;
                previous_ = prev_save;
                (void)first_key;
                (void)first_val;

                int k = expression(-1);
                consume(TOK_COLON, "Expected ':' in dict comprehension.");
                int v = expression(-1);
                state_->emitter.emit_abc(OP_SETINDEX, reg, k, v, line);
                free_reg(v);
                free_reg(k);

                lexer_.restore_state(lex_after);
                current_ = cur_after;
                previous_ = prev_after;

                if (filter_jump >= 0)
                    state_->emitter.patch_jump(filter_jump);
                state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
                state_->emitter.emit_loop(loop_start, 0, line);
                state_->emitter.patch_jump(exit_jump);
                end_scope();
                while (match(TOK_NEWLINE))
                {
                }
                consume(TOK_RBRACE, "Expected '}' after dict comprehension.");
                return reg;
            }

            /* Regular dict */
            state_->emitter.emit_abc(OP_SETINDEX, reg, first_key, first_val, line);
            free_reg(first_val);
            free_reg(first_key);
            while (match(TOK_COMMA))
            {
                while (match(TOK_NEWLINE))
                {
                }
                if (check(TOK_RBRACE))
                    break;
                int k = expression(-1);
                consume(TOK_COLON, "Expected ':' after map key.");
                int v = expression(-1);
                state_->emitter.emit_abc(OP_SETINDEX, reg, k, v, previous_.line);
                free_reg(v);
                free_reg(k);
            }
            while (match(TOK_NEWLINE))
            {
            }
            consume(TOK_RBRACE, "Expected '}' after map entries.");
            return reg;
        }

        /* Set literal or set comprehension: first expr not followed by ':' */
        state_->emitter.shrink_to(off_save);
        state_->next_reg = reg_save;
        vm_->shrink_globals(glob_save);
        state_->emitter.emit_abc(OP_NEWSET, reg, 0, 0, line);
        (void)first_key;

        /* Check for set comprehension: {expr for var in iter} */
        /* Re-parse first expression inside the set logic */
        LexerState lex_before_first = lex_save;
        Token cur_before = cur_save;
        Token prev_before = prev_save;

        lexer_.restore_state(lex_before_first);
        current_ = cur_before;
        previous_ = prev_before;

        int elem0 = expression(-1);
        if (check(TOK_FOR))
        {
            /* Set comprehension */
            state_->emitter.shrink_to(off_save + 1); /* keep NEWSET */
            state_->next_reg = reg_save;
            (void)elem0;

            advance(); /* consume 'for' */
            begin_scope();
            consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
            Token var_name = previous_;
            consume(TOK_IN, "Expected 'in' after variable name.");

            int iter_reg = alloc_reg();
            int ir = parse_precedence(PREC_OR, iter_reg);
            if (ir != iter_reg)
                emit_move(iter_reg, ir);
            int idx_reg = alloc_reg();
            state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);
            int len_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 1, line);
            int var_reg = add_local(var_name);

            int loop_start = state_->emitter.current_offset();
            int cmp_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
            int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
            free_reg(cmp_reg);
            state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

            int filter_jump = -1;
            if (match(TOK_IF))
            {
                int cond = expression(-1);
                filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                free_reg(cond);
            }

            LexerState lex_after = lexer_.save_state();
            Token cur_after = current_;
            Token prev_after = previous_;

            lexer_.restore_state(lex_before_first);
            current_ = cur_before;
            previous_ = prev_before;

            int body = expression(-1);
            state_->emitter.emit_abc(OP_SETADD, reg, body, 0, line);
            free_reg(body);

            lexer_.restore_state(lex_after);
            current_ = cur_after;
            previous_ = prev_after;

            if (filter_jump >= 0)
                state_->emitter.patch_jump(filter_jump);
            state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
            state_->emitter.emit_loop(loop_start, 0, line);
            state_->emitter.patch_jump(exit_jump);
            end_scope();
            while (match(TOK_NEWLINE))
            {
            }
            consume(TOK_RBRACE, "Expected '}' after set comprehension.");
            return reg;
        }

        /* Regular set */
        state_->emitter.emit_abc(OP_SETADD, reg, elem0, 0, line);
        free_reg(elem0);
        while (match(TOK_COMMA))
        {
            while (match(TOK_NEWLINE))
            {
            }
            if (check(TOK_RBRACE))
                break;
            int e = expression(-1);
            state_->emitter.emit_abc(OP_SETADD, reg, e, 0, previous_.line);
            free_reg(e);
        }
        while (match(TOK_NEWLINE))
        {
        }
        consume(TOK_RBRACE, "Expected '}' after set literal.");
        return reg;
    }

    /* =========================================================
    ** Lambda: lambda params: expr
    ** ========================================================= */

    /* =========================================================
    ** yield expression: yield expr
    ** ========================================================= */

    int Compiler::yield_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Mark this function as a generator */
        state_->is_generator = true;

        /* Parse the value to yield */
        int val = expression(reg);
        if (val != reg)
            emit_move(reg, val);

        /* Emit OP_YIELD: A=dest for resumed value, B=value to yield */
        state_->emitter.emit_abc(OP_YIELD, reg, reg, 0, previous_.line);

        return reg;
    }

    int Compiler::await_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Parse the fiber/value to await */
        int val = expression(reg);
        if (val != reg)
            emit_move(reg, val);

        /* Emit OP_AWAIT: A=dest, B=fiber */
        state_->emitter.emit_abc(OP_AWAIT, reg, reg, 0, previous_.line);

        return reg;
    }

    int Compiler::lambda_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        CompilerState fn_state;
        fn_state.parent = state_;
        fn_state.function = new_func(gc_);
        fn_state.emitter = Emitter(gc_);
        fn_state.local_count = 0;
        fn_state.scope_depth = 0;
        fn_state.next_reg = 0;
        fn_state.max_reg = 0;
        fn_state.upvalue_count = 0;
        fn_state.loop_depth = 0;
        fn_state.is_method = false;
        fn_state.is_generator = false;
        fn_state.global_count = 0;

        fn_state.emitter.begin("<lambda>", 0, current_file_);

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters */
        int arity = 0;
        if (!check(TOK_COLON))
        {
            do
            {
                consume(TOK_IDENTIFIER, "Expected parameter name.");
                add_local(previous_);
                arity++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_COLON, "Expected ':' after lambda parameters.");

        /* Body: single expression */
        int result = expression(0);
        if (result != 0)
            emit_move(0, result);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        fn->arity = arity;

        /* Copy upvalue descriptors */
        int nuv = state_->upvalue_count;
        fn->upvalue_count = nuv;
        if (nuv > 0)
        {
            fn->upval_descs = (UpvalDesc *)zen_alloc(gc_, nuv * sizeof(UpvalDesc));
            for (int i = 0; i < nuv; i++)
            {
                fn->upval_descs[i].index = (uint8_t)state_->upvalues[i].index;
                fn->upval_descs[i].is_local = state_->upvalues[i].is_local ? 1 : 0;
            }
        }

        state_ = enclosing;

        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));
        state_->emitter.emit_abx(OP_CLOSURE, reg, ki, previous_.line);
        return reg;
    }

    /* =========================================================
    ** super().method(args) — parent class method invocation
    **
    ** Emits OP_SUPER_INVOKE (3-word instruction):
    **   word1: [OP_SUPER_INVOKE | base | argc | 0]
    **   word2: (selector_slot << 16) | name_ki
    **   word3: parent_class_constant_index
    ** ========================================================= */

    int Compiler::super_expr(int dest)
    {
        if (!in_class_)
        {
            error("Cannot use 'super' outside of a class.");
            return dest >= 0 ? dest : alloc_reg();
        }
        if (!class_has_parent_)
        {
            error("Cannot use 'super' in a class with no parent.");
            return dest >= 0 ? dest : alloc_reg();
        }

        /* Consume super() — the parens are mandatory but empty */
        consume(TOK_LPAREN, "Expected '(' after 'super'.");
        consume(TOK_RPAREN, "Expected ')' after 'super('.");

        /* Must be followed by .method(...) */
        consume(TOK_DOT, "Expected '.method(...)' after 'super()'.");
        consume(TOK_IDENTIFIER, "Expected method name after 'super().'.");
        Token method_name = previous_;

        consume(TOK_LPAREN, "Expected '(' after method name.");

        /* self is the receiver — copy to temp base reg */
        int base = alloc_reg();
        emit_move(base, 0); /* reg 0 = self */

        /* Arguments */
        int nargs = argument_list(base, 0, super_signature(method_name));
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* Resolve parent class as a global → store as constant */
        ObjString *parent_str = token_string(current_class_parent_);
        state_->emitter.add_constant(val_obj((Obj *)parent_str));
        /* At runtime we'll need the actual class object, so store it as a
        ** global-load constant. Actually, the VM handler reads parent from
        ** frame->func->constants[parent_ki]. We need to store the class VALUE
        ** there. Since the class is a global, we emit a special constant that
        ** will be resolved at runtime. Let's store the name and do a global
        ** lookup at runtime. But the existing handler does as_class(K[parent_ki]).
        ** We need to change the approach: store the global index instead. */

        /* Simpler: emit OP_GETGLOBAL into a temp, store that temp's value...
        ** Actually the cleanest: use the parent global index and have the VM
        ** read it from globals at runtime. Let's patch the OP_SUPER_INVOKE
        ** to use global index in word3 instead of constant index. */

        int parent_gidx = find_or_add_global(
            current_class_parent_.start, current_class_parent_.length);

        int sel = vm_->intern_selector(method_name.start, method_name.length);
        int name_ki = state_->emitter.add_string_constant(
            method_name.start, method_name.length);

        state_->emitter.emit_abc(OP_SUPER_INVOKE, base, nargs, 0, method_name.line);
        state_->emitter.emit((uint32_t)((sel << 16) | (name_ki & 0xFFFF)), method_name.line);
        state_->emitter.emit((uint32_t)parent_gidx, method_name.line);

        state_->next_reg = base + 1;
        if (dest >= 0 && dest != base)
        {
            emit_move(dest, base);
            free_reg(base);
            return dest;
        }
        return base;
    }

} /* namespace zen */
