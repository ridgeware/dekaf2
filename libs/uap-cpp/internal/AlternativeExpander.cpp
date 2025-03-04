#include "AlternativeExpander.h"

#include "StringUtils.h"

namespace uap_cpp {

namespace {

bool is_negative_lookahead_operator(const StringView& view) {
  const char* s = view.start();
  if (view.isEnd(s) || view.isEnd(s + 1)) {
    return false;
  }
  return s[0] == '?' && s[1] == '!';
}

bool is_non_capture_operator(const StringView& view) {
  const char* s = view.start();
  if (view.isEnd(s) || view.isEnd(s + 1)) {
    return false;
  }
  return s[0] == '?' && s[1] == ':';
}

typedef std::vector<StringView> Stack;

void expand(const StringView&,
            std::string prefix,
            Stack next_stack,
            std::vector<std::string>&);

bool expand_root_level_alternatives(const StringView& view,
                                    std::string& prefix,
                                    Stack& next,
                                    std::vector<std::string>& out) {
  const char* s = view.start();
  int level = 0;
  bool prev_was_backslash = false;
  while (!view.isEnd(s)) {
    if (!prev_was_backslash) {
      if (*s == '(') {
        ++level;
      } else if (*s == ')') {
        if (level > 0) {
          --level;
        }
      } else if (*s == '[') {
        const char* closing_parenthesis = get_closing_parenthesis(view.from(s));
        if (closing_parenthesis) {
          // Skip character-level alternative block
          s = closing_parenthesis;
          continue;
        }
      }

      if (level == 0 && *s == '|') {
        // Go through alternative on the left
        expand(view.to(s), prefix, next, out);

        // Go through alternative(s) on the right
        expand(view.from(s + 1), std::move(prefix), std::move(next), out);

        return true;
      }
    }

    prev_was_backslash = *s == '\\' && !prev_was_backslash;
    ++s;
  }

  return false;
}

bool expand_root_level_parentheses(const StringView& view,
                                   std::string& prefix,
                                   Stack& next,
                                   std::vector<std::string>& out) {
  const char* s = view.start();
  int level = 0;
  bool prev_was_backslash = false;
  while (!view.isEnd(s)) {
    if (!prev_was_backslash) {
      if (*s == '(') {
        ++level;
        if (level == 1) {
          const char* closing_parenthesis =
              get_closing_parenthesis(view.from(s));
          if (!closing_parenthesis) {
            // Bad expression
            --level;
            ++s;
            continue;
          }

          if (is_optional_operator(view.from(closing_parenthesis + 1)) ||
              is_negative_lookahead_operator(view.from(s + 1))) {
            // Continue after parentheses
            s = closing_parenthesis;
            continue;
          }

          // Add ( or (?: to prefix
          const char* inner_start = s + 1;
          if (is_non_capture_operator(view.from(inner_start))) {
            inner_start += 2;
          }
          prefix.append(view.start(), inner_start - view.start());

          next.emplace_back(view.from(closing_parenthesis));

          // Recursively go through what is enclosed by parentheses
          expand(StringView(inner_start, closing_parenthesis),
                 std::move(prefix),
                 std::move(next),
                 out);
          return true;
        }
      } else if (*s == ')') {
        if (level > 0) {
          --level;
        }
      } else if (*s == '[') {
        const char* closing_parenthesis = get_closing_parenthesis(view.from(s));
        if (closing_parenthesis) {
          // Skip character-level alternative block
          s = closing_parenthesis;
          continue;
        }
      }
    }

    prev_was_backslash = *s == '\\' && !prev_was_backslash;
    ++s;
  }

  // No alternatives or parentheses, so add to to prefix
  prefix.append(view.start(), s - view.start());

  return false;
}

void expand(const StringView& view,
            std::string prefix,
            Stack next,
            std::vector<std::string>& out) {
  if (expand_root_level_alternatives(view, prefix, next, out)) {
    // Root-level alternatives were handled recursively
    return;
  }

  if (expand_root_level_parentheses(view, prefix, next, out)) {
    // Root-level parentheses where handled recursively
    return;
  }

  if (next.empty()) {
    // Reached end of string, add what has been collected
    out.emplace_back(std::move(prefix));
  } else {
    // Pop the stack and continue with rest of string
    StringView next_view = next.back();
    next.pop_back();

    expand(next_view, std::move(prefix), std::move(next), out);
  }
}

}  // namespace

std::vector<std::string> AlternativeExpander::expand(
    const StringView& expression) {
  std::vector<std::string> out;
  std::string prefix;
  Stack next;
  uap_cpp::expand(expression, std::move(prefix), std::move(next), out);
  return out;
}

}  // namespace uap_cpp
