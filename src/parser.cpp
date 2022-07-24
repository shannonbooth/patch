class Parser {
public:
    explicit Parser(const std::string& line)
        : m_current(line.cbegin())
        , m_end(line.cend())
        , m_line(line)
    {
    }
    bool is_eof() const
    {
        return m_current == m_end;
    }
    char peek() const
    {
        if (m_current == m_end)
            return '\0';
        return *m_current;
    }
    char consume()
    {
        char c = *m_current;
        ++m_current;
        return c;
    }
    bool consume_specific(char c)
    {
        if (m_current == m_end)
            return false;
        if (*m_current != c)
            return false;

        ++m_current;
        return true;
    }

    bool consume_specific(const char* chars)
    {
        auto current = m_current;
        for (const char* c = chars; *c != '\0'; ++c) {
            if (!consume_specific(*c)) {
                m_current = current;
                return false;
        return true;
    bool consume_uint()
    {
        if (m_current == m_end)
            return false;
        if (!is_digit(*m_current))
            return false;

        // Skip past this digit we just checked, and any remaining parts of the integer.
        ++m_current;
        m_current = std::find_if(m_current, m_end, is_not_digit);
        return true;
    }

    bool consume_line_number(LineNumber& output)
    {
        auto start = m_current;
        if (!consume_uint())
            return false;

        return string_to_line_number(std::string(start, m_current), output);
    }

    std::string::const_iterator current() const { return m_current; }
    std::string::const_iterator end() const { return m_end; }

    std::string parse_quoted_string()
    {
        std::string output;

        consume_specific('"');

        while (!is_eof()) {
            // Reached the end of the string
            if (peek() == '"')
                return output;

            // Some escaped character, peek past to determine the intended unescaped character.
            if (consume_specific('\\')) {
                if (is_eof())
                    throw std::invalid_argument("Invalid unterminated \\ in quoted path " + m_line);

                switch (consume()) {
                case '\\':
                    output += '\\';
                    break;
                case '"':
                    output += '"';
                    break;
                case 'n':
                    output += '\n';
                    break;
                case 't':
                    output += '\t';
                    break;
                default:
                    throw std::invalid_argument("Invalid or unsupported escape character in path " + m_line);
                }
            } else {
                // Normal case - a character of the path we can just add to our output.
                output += consume();
            }
        }

        throw std::invalid_argument("Failed to find terminating \" when parsing " + m_line);
    }

private:
    std::string::const_iterator m_current;
    std::string::const_iterator m_end;
    const std::string& m_line;
};
        Parser parser(input);
        path = parser.parse_quoted_string();
        it = parser.current();
static uint16_t parse_mode(const std::string& mode_str)
{
    if (mode_str.size() != 6)
        throw std::invalid_argument("mode string not of correct size: " + mode_str);

    return static_cast<uint16_t>(std::stoul(mode_str, nullptr, 8));
}

            Parser parser(name);
            output = parser.parse_quoted_string();
        patch.old_file_mode = parse_mode(line.substr(18, line.size() - 18));
        patch.new_file_mode = parse_mode(line.substr(14, line.size() - 14));
        return true;
    }

    if (starts_with(line, "old mode ")) {
        patch.old_file_mode = parse_mode(line.substr(9, line.size() - 9));
        return true;
    }

    if (starts_with(line, "new mode ")) {
        patch.new_file_mode = parse_mode(line.substr(9, line.size() - 9));
    if (starts_with(line, "index ")) {
        return true;
    }

    // NOTE: GIT binary patch line not included as part of header info.
    if (starts_with(line, "GIT binary patch")) {
        patch.operation = Operation::Binary;
        return false;
    }

static void parse_git_header_name(const std::string& line, Patch& patch, int strip)
{
    Parser parser(line);
    if (!parser.consume_specific("diff --git "))
        return;

    std::string name;
    if (parser.peek() == '"') {
        name = parser.parse_quoted_string();
    } else {
        while (!parser.is_eof()) {
            if (parser.consume_specific(" b/"))
                break;
            name += parser.consume();
        }
    }

    name = strip_path(name, strip);
    patch.old_file_path = name;
    patch.new_file_path = name;
}

            parse_git_header_name(line, patch, strip);
static Hunk hunk_from_context_parts(LineNumber old_start_line, const std::vector<PatchLine>& old_lines,
    auto append_content = [&](std::vector<PatchLine>& lines, LineNumber start_line, LineNumber end_line) {
    if (state == State::InitialHunkContext && patch.hunks.empty())
            assert(!current_hunk->lines.empty()); // precondition checked by `parse_patch_header`