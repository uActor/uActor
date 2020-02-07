#ifndef MAIN_INCLUDE_STRING_HELPER_HPP_
#define MAIN_INCLUDE_STRING_HELPER_HPP_

#include <list>
#include <string_view>
#include <utility>

struct StringHelper {
  // Adapted string_view substring algorithm from
  // https://www.bfilipek.com/2018/07/string-view-perf-followup.html
  static std::list<std::string_view> string_split(std::string_view input,
                                                  const char* split = ",") {
    size_t pos = 0;
    std::list<std::string_view> substrings;
    while (pos < input.size()) {
      const auto end_pos = input.find_first_of(split, pos);
      if (pos != end_pos) {
        substrings.emplace_back(input.substr(pos, end_pos - pos));
      }
      if (end_pos == std::string_view::npos) {
        break;
      }
      pos = end_pos + 1;
    }
    return std::move(substrings);
  }
};

#endif  //  MAIN_INCLUDE_STRING_HELPER_HPP_
