/*
 * Copyright 2008 Google Inc. All Rights Reserved.
 * Author: fraser@google.com (Neil Fraser)
 * Author: mikeslemmer@gmail.com (Mike Slemmer)
 * Author: snhere@gmail.com (Sergey Nozhenko)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Diff Match and Patch
 * http://code.google.com/p/google-diff-match-patch/
 */

#ifndef DIFF_MATCH_PATCH_H
#define DIFF_MATCH_PATCH_H

#include <limits>
#include <list>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <time.h>
#include <cinttypes>

/*
 * Functions for diff, match and patch.
 * Computes the difference between two texts to create a patch.
 * Applies the patch onto another text, allowing for errors.
 *
 * @author fraser@google.com (Neil Fraser)
 *
 * Qt/C++ port by mikeslemmer@gmail.com (Mike Slemmer)
 *
 * STL-only port by snhere@gmail.com (Sergey Nozhenko)
 * and some tweaks for std::string by leutloff@sundancer.oche.de (Christian Leutloff)
 *
 * Adding option to configure with a string view class in addition to a string class,
 * increasing speed by about 25%, by jschurig@translations.com (Joachim Schurig)
 *
 * Here is a trivial sample program:
 *

 #include "diff_match_patch.h"
 #include <string>
 #include <string_view>
 using namespace std;
 int main(int argc, char **argv) {
   diff_match_patch<wstring, wstring_view> dmp;
   wstring str1 = L"First string in diff";
   wstring str2 = L"Second string in diff";

   wstring strPatch = dmp.patch_toText(dmp.patch_make(str1, str2));
   pair<wstring, vector<bool> > out
       = dmp.patch_apply(dmp.patch_fromText(strPatch), str1);
   wstring strResult = out.first;

   // here, strResult will equal str2 above.
   return 0;
 }

 */

// Character type dependencies
template <class char_t, class string_t, class stringview_t> struct diff_match_patch_traits {};

/**
 * Class containing the diff, match and patch methods.
 * Also contains the behaviour settings.
 */
template <class stringT = std::wstring, class stringviewT = stringT, class traits = diff_match_patch_traits<typename stringT::value_type, stringT, stringviewT> >
class diff_match_patch
{
 public:
  /**
  * String and character types
  */
  typedef stringT     string_t;
  typedef stringviewT stringview_t;
  typedef typename string_t::value_type char_t;
  typedef typename string_t::size_type strlen_t;
  typedef int32_t     step_t;
  typedef uint16_t    score_t; // scores range from 0..6
  typedef uint16_t    padding_t;

  static constexpr strlen_t npos { static_cast<strlen_t>(-1) };

  /**
  * The data structure representing a diff is a Linked list of Diff objects:
  * {Diff(Operation.DELETE, "Hello"), Diff(Operation.INSERT, "Goodbye"),
  *  Diff(Operation.EQUAL, " world.")}
  * which means: delete "Hello", add "Goodbye" and keep " world."
  */
  enum Operation : uint16_t
  {
    EQUAL  = 0,
    INSERT = 1,
    DELETE = 2
  };

  /**
   * the diff mode
   *
   */
  enum Mode
  {
    CHARACTER           = 0,
    WORD                = 1,
    LINE_THEN_CHARACTER = 2,
    LINE                = 3
  };

  /**
  * Class representing one diff operation.
  */
  class Diff
  {
   public:
    /// One of: INSERT, DELETE or EQUAL.
    Operation operation { EQUAL };

    /// The text associated with this diff operation.
    string_t text;

    Diff() = default;
    /**
     * Constructor.  Initializes the diff with the provided values.
     * @param operation One of INSERT, DELETE or EQUAL.
     * @param text The text being applied.
     */
    Diff(Operation _operation, string_t _text)
	: operation(_operation), text(std::move(_text))
	{}

	template<class StringView,
             typename std::enable_if<!std::is_same<string_t, StringView>::value, int>::type = 0>
    Diff(Operation _operation, StringView _text)
	: operation(_operation), text(_text.data(), _text.size())
    {}

    /**
     * Display a human-readable version of this Diff.
     * @return text version.
     */
    string_t toString() const
    {
      string_t prettyText;
      prettyText.reserve(text.size() + 16);
      prettyText = traits::cs(L"Diff(");
      prettyText += strOperation(operation);
      prettyText += traits::cs(L",\"");
      auto iStart = prettyText.size();
      // Replace linebreaks with Pilcrow signs.
      for (auto& ch : text)
      {
        if (ch == traits::eol)
        {
          prettyText += traits::pilcrow;
        }
        else
        {
          prettyText += ch;
        }
      }
      prettyText += traits::cs(L"\")");
      return prettyText;
    }

    /**
     * Is this Diff equivalent to another Diff?
     * @param d Another Diff to compare against
     * @return true or false
     */
    bool operator==(const Diff &d) const
    {
      return (d.operation == operation) && (d.text == text);
    }

    bool operator!=(const Diff &d) const
    {
      return !(operator == (d));
    }

    static string_t strOperation(Operation op)
    {
      switch (op)
      {
        case EQUAL:
          return traits::cs(L"EQUAL");
        case INSERT:
          return traits::cs(L"INSERT");
        case DELETE:
          return traits::cs(L"DELETE");
      }
      throw string_t(traits::cs(L"Invalid operation."));
    }

  }; // class Diff

  typedef std::list<Diff> Diffs;


  /**
  * Class representing one patch operation.
  */
  class Patch
  {
   public:
    Diffs    diffs;
    strlen_t start1  { 0 };
    strlen_t start2  { 0 };
    strlen_t length1 { 0 };
    strlen_t length2 { 0 };

    /**
     * Emulate GNU diff's format.
     * Header: @@ -382,8 +481,9 @@
     * Indices are printed as 1-based, not 0-based.
     * @return The GNU diff string
     */
    string_t toString() const
    {
      string_t coords1, coords2;
      if (length1 == 0) {
        coords1 = traits::to_string(start1);
        coords1 += traits::cs(L",0");
      } else if (length1 == 1) {
        coords1 = traits::to_string(start1 + 1);
      } else {
        coords1 = traits::to_string(start1 + 1);
        coords1 += traits::from_wchar(L',');
        coords1 += traits::to_string(length1);
      }
      if (length2 == 0) {
		  coords2 = traits::to_string(start2);
		  coords2 += traits::cs(L",0");
      } else if (length2 == 1) {
        coords2 = traits::to_string(start2 + 1);
      } else {
        coords2 = traits::to_string(start2 + 1);
        coords2 += traits::from_wchar(L',');
        coords2 += traits::to_string(length2);
      }
		string_t text(traits::cs(L"@@ -"));
        text += coords1;
        text += traits::cs(L" +");
        text += coords2;
        text += traits::cs(L" @@\n");
      // Escape the body of the patch with %xx notation.
      for (const auto& cur_diff : diffs)
	  {
        switch (cur_diff.operation) {
          case INSERT:
            text += traits::from_wchar(L'+');
            break;
          case DELETE:
            text += traits::from_wchar(L'-');
            break;
          case EQUAL:
            text += traits::from_wchar(L' ');
            break;
        }
        append_percent_encoded(text, cur_diff.text);
        text += traits::from_wchar(L'\n');
      }

      return text;
    }

  }; // class Patch

  typedef std::list<Patch> Patches;

  friend class diff_match_patch_test;

 public:

  // The number of bits in an int - this one is constexpr and only depends
  // on the type chosen for the diff stepping
  static constexpr uint16_t Match_MaxBits { sizeof(step_t) * 8 };

  // Defaults.

  // Set these on your diff_match_patch instance to override the defaults.

  // At what point is no match declared (0.0 = perfection, 1.0 = very loose).
  float Match_Threshold       { 0.5f };
  // How far to search for a match (0 = exact location, 1000+ = broad match).
  // A match this many characters away from the expected location will add
  // 1.0 to the score (0.0 is a perfect match).
  strlen_t Match_Distance     { 1000 };
  // When deleting a large block of text (over ~64 characters), how close does
  // the contents have to match the expected contents. (0.0 = perfection,
  // 1.0 = very loose).  Note that Match_Threshold controls how closely the
  // end points of a delete need to match.
  float Patch_DeleteThreshold { 0.5f };
  // Chunk size for context length.
  uint16_t Patch_Margin       { 4 };

private:

  // used in patch generation, not settable
  uint16_t MaxMatchPattern    { static_cast<uint16_t>((2 * Patch_Margin > Match_MaxBits)
                                                      ? 0
                                                      : Match_MaxBits - 2 * Patch_Margin) };

public:

  //  DIFF FUNCTIONS

  /**
   * Find the differences between two texts.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param checklines Speedup flag.  If false, then don't run a
   *     line-level diff first to identify the changed areas.
   *     If true, then run a faster slightly less optimal diff.
   *     Most of the time checklines is wanted, so default to true.
   * @return Linked List of Diff objects.
   */
  static Diffs diff_main(const stringview_t &text1, const stringview_t &text2, Mode DiffMode = Mode::LINE_THEN_CHARACTER, float fTimeout = 1.0f)
  {
    // Set a deadline by which time the diff must be complete.
    clock_t deadline;
    if (fTimeout <= 0) {
      deadline = std::numeric_limits<clock_t>::max();
    } else {
      deadline = clock() + static_cast<clock_t>(fTimeout * CLOCKS_PER_SEC);
    }
    Diffs diffs;
    diff_main(text1, text2, DiffMode, deadline, diffs);
    return diffs;
  }

  /**
   * Find the differences between two texts.  Simplifies the problem by
   * stripping any common prefix or suffix off the texts before diffing.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param checklines Speedup flag.  If false, then don't run a
   *     line-level diff first to identify the changed areas.
   *     If true, then run a faster slightly less optimal diff.
   * @param deadline Time when the diff should be complete by.  Used
   *     internally for recursive calls.  Users should set DiffTimeout instead.
   * @param Linked List of Diff objects.
   */
 private:
  static void diff_main(const stringview_t &text1, const stringview_t &text2, Mode DiffMode, clock_t deadline, Diffs& diffs)
  {
    diffs.clear();

    // Check for equality (speedup).
    if (text1 == text2) {
      if (!text1.empty()) {
        diffs.push_back(Diff(EQUAL, text1));
      }
    }
    else
    {
      // Trim off common prefix (speedup).
      strlen_t commonlength = diff_commonPrefix(text1, text2);
      const stringview_t commonprefix = text1.substr(0, commonlength);
      stringview_t textChopped1 = text1.substr(commonlength);
      stringview_t textChopped2 = text2.substr(commonlength);

      // Trim off common suffix (speedup).
      commonlength = diff_commonSuffix(textChopped1, textChopped2);
      const stringview_t commonsuffix = traits::right(textChopped1, commonlength);
      textChopped1 = textChopped1.substr(0, textChopped1.length() - commonlength);
      textChopped2 = textChopped2.substr(0, textChopped2.length() - commonlength);

      // Compute the diff on the middle block.
      diff_compute(textChopped1, textChopped2, DiffMode, deadline, diffs);

      // Restore the prefix and suffix.
      if (!commonprefix.empty()) {
        diffs.push_front(Diff(EQUAL, commonprefix));
      }
      if (!commonsuffix.empty()) {
        diffs.push_back(Diff(EQUAL, commonsuffix));
      }

      diff_cleanupMerge(diffs);
    }
  }

  /**
   * Find the differences between two texts.  Assumes that the texts do not
   * have any common prefix or suffix.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param checklines Speedup flag.  If false, then don't run a
   *     line-level diff first to identify the changed areas.
   *     If true, then run a faster slightly less optimal diff.
   * @param deadline Time when the diff should be complete by.
   * @param Linked List of Diff objects.
   */
 private:
  static void diff_compute(const stringview_t &text1, const stringview_t &text2, Mode DiffMode, clock_t deadline, Diffs& diffs)
  {
    if (text1.empty()) {
      // Just add some text (speedup).
      diffs.push_back(Diff(INSERT, text2));
      return;
    }

    if (text2.empty()) {
      // Just delete some text (speedup).
      diffs.push_back(Diff(DELETE, text1));
      return;
    }

    {
      const stringview_t& longtext  = text1.length() > text2.length() ? text1 : text2;
      const stringview_t& shorttext = text1.length() > text2.length() ? text2 : text1;
      const typename stringview_t::size_type i = longtext.find(shorttext);
      if (i != stringview_t::npos) {
        // Shorter text is inside the longer text (speedup).
        const Operation op = (text1.length() > text2.length()) ? DELETE : INSERT;
        diffs.push_back(Diff(op, longtext.substr(0, i)));
        diffs.push_back(Diff(EQUAL, shorttext));
        diffs.push_back(Diff(op, traits::mid(longtext, i + shorttext.length())));
        return;
      }

      if (shorttext.length() == 1) {
        // Single character string.
        // After the previous speedup, the character can't be an equality.
        diffs.push_back(Diff(DELETE, text1));
        diffs.push_back(Diff(INSERT, text2));
        return;
      }
      // Garbage collect longtext and shorttext by scoping out.
    }

    // Don't risk returning a non-optimal diff if we have unlimited time.
    if (deadline != std::numeric_limits<clock_t>::max())
    {
      // Check to see if the problem can be split in two.
      HalfMatchResult hm;
      if (diff_halfMatch(text1, text2, hm)) {
        // A half-match was found, sort out the return data.
        // Send both pairs off for separate processing.
        diff_main(hm.text1_a, hm.text2_a, DiffMode, deadline, diffs);
        diffs.push_back(Diff(EQUAL, hm.mid_common));
        Diffs diffs_b;
        diff_main(hm.text1_b, hm.text2_b, DiffMode, deadline, diffs_b);
        diffs.splice(diffs.end(), diffs_b);
        return;
      }
    }

    // Perform a real diff.
    if ((DiffMode == Mode::LINE || DiffMode == Mode::LINE_THEN_CHARACTER)
		&& text1.length() > 100 && text2.length() > 100)
    {
      diff_lineMode(string_t(text1), string_t(text2), DiffMode, deadline, diffs);
      return;
    }

    diff_bisect(text1, text2, deadline, diffs);
  }

  /**
   * Do a quick line-level diff on both strings, then rediff the parts for
   * greater accuracy.
   * This speedup can produce non-minimal diffs.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param deadline Time when the diff should be complete by.
   * @param Linked List of Diff objects.
   */
 private:
  static void diff_lineMode(string_t text1, string_t text2, Mode DiffMode, clock_t deadline, Diffs& diffs)
  {
    // Scan the text on a line-by-line basis first.
    Lines linearray = diff_linesToChars(text1, text2);

    // Now process the "characters"
    diff_main(text1, text2, Mode::CHARACTER, deadline, diffs);

    // Convert the diff back to original text.
    diff_charsToLines(diffs, linearray);
    // Eliminate freak matches (e.g. blank lines)
    diff_cleanupSemantic(diffs);

    if (DiffMode == Mode::LINE_THEN_CHARACTER)
    {
      // Rediff any replacement blocks, this time character-by-character.
      // Add a dummy entry at the end.
      diffs.push_back(Diff(EQUAL, string_t()));
      int count_delete = 0;
      int count_insert = 0;
      string_t text_delete;
      string_t text_insert;

      for (typename Diffs::iterator cur_diff = diffs.begin(); cur_diff != diffs.end(); ++cur_diff)
      {
        switch ((*cur_diff).operation)
        {
          case INSERT:
            count_insert++;
            text_insert += (*cur_diff).text;
            break;

          case DELETE:
            count_delete++;
            text_delete += (*cur_diff).text;
            break;

          case EQUAL:
            // Upon reaching an equality, check for prior redundancies.
            if (count_delete >= 1 && count_insert >= 1)
            {
              // Delete the offending records and add the merged ones.
              typename Diffs::iterator last = cur_diff;
              std::advance(cur_diff, -(count_delete + count_insert));
              cur_diff = diffs.erase(cur_diff, last);

              Diffs new_diffs;
              diff_main(text_delete, text_insert, Mode::CHARACTER, deadline, new_diffs);
              diffs.splice(cur_diff++, new_diffs);
              --cur_diff;
            }
            count_insert = 0;
            count_delete = 0;
            text_delete.clear();
            text_insert.clear();
            break;
        }
      }
      diffs.pop_back();  // Remove the dummy entry at the end.
    }
  }

  /**
   * Find the 'middle snake' of a diff, split the problem in two
   * and return the recursively constructed diff.
   * See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @return Linked List of Diff objects.
   */
 protected:
  static Diffs diff_bisect(const stringview_t &text1, const stringview_t &text2, clock_t deadline)
  {
    Diffs diffs;
    diff_bisect(text1, text2, deadline, diffs);
    return diffs;
  }
 private:
  static void diff_bisect(const stringview_t &text1, const stringview_t &text2, clock_t deadline, Diffs& diffs)
  {
    if (text1.length() + text2.length() > static_cast<typename stringview_t::size_type>(std::numeric_limits<step_t>::max()))
    {
      throw string_t(traits::cs(L"input strings too large."));
    }

    // Cache the text lengths to prevent multiple calls.
    const step_t text1_length = text1.length();
    const step_t text2_length = text2.length();
    const step_t max_d        = (text1_length + text2_length + 1) / 2;
    const step_t v_offset     = max_d;
    const step_t v_length     = 2 * max_d;

    std::vector<step_t> v1(v_length, -1), v2(v_length, -1);
    v1[v_offset + 1] = 0;
    v2[v_offset + 1] = 0;

    const step_t delta = text1_length - text2_length;

    // If the total number of characters is odd, then the front path will
    // collide with the reverse path.
    const bool front = (delta % 2 != 0);

    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    step_t k1start   = 0;
    step_t k1end     = 0;
    step_t k2start   = 0;
    step_t k2end     = 0;

    for (step_t d = 0; d < max_d; ++d) {

      // Bail out if deadline is reached.
      if (clock() > deadline) {
        break;
      }

      // Walk the front path one step.
      for (step_t k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
        const step_t k1_offset = v_offset + k1;
        step_t x1;
        if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
          x1 = v1[k1_offset + 1];
        } else {
          x1 = v1[k1_offset - 1] + 1;
        }
        step_t y1 = x1 - k1;
        while (x1 < text1_length && y1 < text2_length
            && text1[x1] == text2[y1]) {
          x1++;
          y1++;
        }
        v1[k1_offset] = x1;
        if (x1 > text1_length) {
          // Ran off the right of the graph.
          k1end += 2;
        } else if (y1 > text2_length) {
          // Ran off the bottom of the graph.
          k1start += 2;
        } else if (front) {
          step_t k2_offset = v_offset + delta - k1;
          if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
            // Mirror x2 onto top-left coordinate system.
            step_t x2 = text1_length - v2[k2_offset];
            if (x1 >= x2) {
              // Overlap detected.
              diff_bisectSplit(text1, text2, x1, y1, deadline, diffs);
              return;
            }
          }
        }
      }

      // Walk the reverse path one step.
      for (step_t k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
        const step_t k2_offset = v_offset + k2;
        step_t x2;
        if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
          x2 = v2[k2_offset + 1];
        } else {
          x2 = v2[k2_offset - 1] + 1;
        }
        step_t y2 = x2 - k2;
        while (x2 < text1_length && y2 < text2_length
            && text1[text1_length - x2 - 1] == text2[text2_length - y2 - 1]) {
          x2++;
          y2++;
        }
        v2[k2_offset] = x2;
        if (x2 > text1_length) {
          // Ran off the left of the graph.
          k2end += 2;
        } else if (y2 > text2_length) {
          // Ran off the top of the graph.
          k2start += 2;
        } else if (!front) {
          step_t k1_offset = v_offset + delta - k2;
          if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
            step_t x1 = v1[k1_offset];
            step_t y1 = v_offset + x1 - k1_offset;
            // Mirror x2 onto top-left coordinate system.
            x2 = text1_length - x2;
            if (x1 >= x2) {
              // Overlap detected.
              diff_bisectSplit(text1, text2, x1, y1, deadline, diffs);
              return;
            }
          }
        }
      }
    }

    // Diff took too long and hit the deadline or
    // number of diffs equals number of characters, no commonality at all.
    diffs.clear();
    diffs.push_back(Diff(DELETE, text1));
    diffs.push_back(Diff(INSERT, text2));
  }

  /**
   * Given the location of the 'middle snake', split the diff in two parts
   * and recurse.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param x Index of split point in text1.
   * @param y Index of split point in text2.
   * @param deadline Time at which to bail if not yet complete.
   * @return LinkedList of Diff objects.
   */
 private:
  static void diff_bisectSplit(const stringview_t &text1, const stringview_t &text2, step_t x, step_t y, clock_t deadline, Diffs& diffs)
  {
    stringview_t text1a = text1.substr(0, x);
    stringview_t text2a = text2.substr(0, y);
    stringview_t text1b = traits::mid(text1, x);
    stringview_t text2b = traits::mid(text2, y);

    // Compute both diffs serially.
    diff_main(text1a, text2a, Mode::CHARACTER, deadline, diffs);
    Diffs diffs_b;
    diff_main(text1b, text2b, Mode::CHARACTER, deadline, diffs_b);
    diffs.splice(diffs.end(), diffs_b);
  }

  /**
   * Split two texts into a list of strings.  Reduce the texts to a string of
   * hashes where each Unicode character represents one line.
   * @param text1 First string.
   * @param text2 Second string.
   * @return Lines object, containing the encoded text1, the
   *     encoded text2 and the List of pointers to unique strings.  The zeroth element
   *     of the List of unique strings is intentionally blank.
   */
 protected:

  struct Lines : std::vector<stringview_t>
  {
	string_t text1, text2;
  };

  typedef std::unordered_map<stringview_t, std::size_t> lineHash_t;

  static Lines diff_linesToChars(string_t &text1, string_t &text2)
  {
    Lines lineArray;

    lineHash_t lineHash;
    lineArray.text1 = std::move(text1);
    lineArray.text2 = std::move(text2);
    // e.g. linearray[4] == "Hello\n"
    // e.g. linehash.get("Hello\n") == 4

    // "\x00" is a valid character, but various debuggers don't like it.
    // So we'll insert a junk entry to avoid generating a null character.

    text1 = diff_linesToCharsMunge(lineArray.text1, lineHash);
    text2 = diff_linesToCharsMunge(lineArray.text2, lineHash);

    lineArray.resize(lineHash.size() + 1);

    for (const auto& i : lineHash)
	{
      lineArray[i.second] = i.first;
	}

    return lineArray;
  }

  /**
   * Split a text into a list of pointers to strings.  Reduce the texts to a string of
   * hashes where each Unicode character represents one line.
   * @param text String to encode.
   * @param lineHash Map of string pointers to indices.
   * @return Encoded string.
   */
 private:

  static string_t diff_linesToCharsMunge(const string_t &text, lineHash_t &lineHash)
  {
    string_t chars;
    // Walk the text, pulling out a substring for each line.
    // text.split('\n') would would temporarily double our memory footprint.
    // Modifying text would create many large strings to garbage collect.
    typename string_t::size_type lineLen;
    for (typename string_t::const_pointer lineStart = text.data(), textEnd = lineStart + text.size(); lineStart < textEnd; lineStart += lineLen + 1) {
      lineLen = next_token(text, traits::eol, lineStart);
      if (lineStart + lineLen == textEnd) --lineLen;
      stringview_t sLine(lineStart, lineLen + 1);
      chars += (char_t)(*lineHash.insert(std::make_pair(sLine, lineHash.size() + 1)).first).second;
    }
    return chars;
  }

  /**
   * Rehydrate the text in a diff from a string of line hashes to real lines of
   * text.
   * @param diffs LinkedList of Diff objects.
   * @param lineArray List of pointers to unique strings.
   */
 private:
  static void diff_charsToLines(Diffs &diffs, const Lines& lineArray)
  {
    for (auto& cur_diff : diffs)
	{
      string_t text;
      for (step_t y = 0; y < static_cast<step_t>(cur_diff.text.length()); y++) {
        stringview_t sLine = lineArray[static_cast<std::size_t>(cur_diff.text[y])];
        text.append(sLine);
      }
      cur_diff.text = std::move(text);
    }
  }

  /**
   * Determine the common prefix of two strings.
   * @param text1 First string.
   * @param text2 Second string.
   * @return The number of characters common to the start of each string.
   */
 public:
  static strlen_t diff_commonPrefix(const stringview_t &text1, const stringview_t &text2) {
    // Performance analysis: http://neil.fraser.name/news/2007/10/09/
	return (text1.length() <= text2.length()) ? std::mismatch(text1.begin(), text1.end(), text2.begin()).first - text1.begin()
	                                          : std::mismatch(text2.begin(), text2.end(), text1.begin()).first - text2.begin();
  }

  /**
   * Determine the common suffix of two strings.
   * @param text1 First string.
   * @param text2 Second string.
   * @return The number of characters common to the end of each string.
   */
 public:
  static strlen_t diff_commonSuffix(const stringview_t &text1, const stringview_t &text2) {
    // Performance analysis: http://neil.fraser.name/news/2007/10/09/
	return (text1.length() <= text2.length()) ? std::mismatch(text1.rbegin(), text1.rend(), text2.rbegin()).first - text1.rbegin()
	                                          : std::mismatch(text2.rbegin(), text2.rend(), text1.rbegin()).first - text2.rbegin();
  }

  /**
   * Determine if the suffix of one string is the prefix of another.
   * @param text1 First string.
   * @param text2 Second string.
   * @return The number of characters common to the end of the first
   *     string and the start of the second string.
   */
 protected:
  static strlen_t diff_commonOverlap(const stringview_t &text1, const stringview_t &text2) {
    // Cache the text lengths to prevent multiple calls.
    const strlen_t text1_length = text1.length();
    const strlen_t text2_length = text2.length();
    // Eliminate the null case.
    if (text1_length == 0 || text2_length == 0) {
      return 0;
    }
    // Truncate the longer string.
	stringview_t text1_trunc;
    stringview_t text2_trunc;
    if (text1_length > text2_length) {
      text1_trunc = traits::right(text1, text2_length);
      text2_trunc = text2;
    } else if (text1_length < text2_length) {
      text1_trunc = text1;
      text2_trunc = text2.substr(0, text1_length);
	} else {
      text1_trunc = text1;
      text2_trunc = text2;
	}
    const strlen_t text_length = std::min(text1_length, text2_length);
    // Quick check for the worst case.
    if (text1_trunc == text2_trunc) {
      return text_length;
    }

    // Start by looking for a single character match
    // and increase length until no match is found.
    // Performance analysis: http://neil.fraser.name/news/2010/11/04/
    strlen_t best = 0;
    strlen_t length = 1;
    while (true) {
      stringview_t pattern = traits::right(text1_trunc, length);
      strlen_t found = text2_trunc.find(pattern);
      if (found == npos) {
        return best;
      }
      length += found;
      if (found == 0 || traits::right(text1_trunc, length) == text2_trunc.substr(0, length)) {
        best = length;
        length++;
      }
    }
  }

  /**
   * Do the two texts share a substring which is at least half the length of
   * the longer text?
   * This speedup can produce non-minimal diffs.
   * @param text1 First string.
   * @param text2 Second string.
   * @param HalfMatchResult object, containing the prefix of text1, the
   *     suffix of text1, the prefix of text2, the suffix of text2 and the
   *     common middle.  
   * @return Boolean true if there was a match, false otherwise.
   */
 protected:
  struct HalfMatchResult
  {
    stringview_t text1_a, text1_b, text2_a, text2_b, mid_common;
  };

  static bool diff_halfMatch(const stringview_t &text1, const stringview_t &text2, HalfMatchResult& hm)
  {
    const stringview_t& longtext  = text1.length() > text2.length() ? text1 : text2;
    const stringview_t& shorttext = text1.length() > text2.length() ? text2 : text1;
    if (longtext.length() < 4 || shorttext.length() * 2 < longtext.length()) {
      return false;  // Pointless.
    }

    HalfMatchResult res1, res2;
    // First check if the second quarter is the seed for a half-match.
    bool hm1 = diff_halfMatchI(longtext, shorttext, (longtext.length() + 3) / 4, res1);
    // Check again based on the third quarter.
    bool hm2 = diff_halfMatchI(longtext, shorttext, (longtext.length() + 1) / 2, res2);
    if (!hm1 && !hm2) {
      return false;
    } else if (!hm2) {
      hm = std::move(res1);
    } else if (!hm1) {
      hm = std::move(res2);
    } else {
      // Both matched.  Select the longest.
      hm = (res1.mid_common.length() > res2.mid_common.length() ? std::move(res1) : std::move(res2));
    }

    // A half-match was found, sort out the return data.
    if (text1.length() <= text2.length()) {
      hm.text1_a.swap(hm.text2_a);
      hm.text1_b.swap(hm.text2_b);
    }
    return true;
  }

  /**
   * Does a substring of shorttext exist within longtext such that the
   * substring is at least half the length of longtext?
   * @param longtext Longer string.
   * @param shorttext Shorter string.
   * @param i Start index of quarter length substring within longtext.
   * @param HalfMatchResult object, containing the prefix of longtext, the
   *     suffix of longtext, the prefix of shorttext, the suffix of shorttext
   *     and the common middle.
   * @return Boolean true if there was a match, false otherwise.
   */
 private:
  static bool diff_halfMatchI(const stringview_t &longtext, const stringview_t &shorttext, strlen_t i, HalfMatchResult& best)
  {
    // Start with a 1/4 length substring at position i as a seed.
    const stringview_t seed = traits::mid(longtext, i, longtext.length() / 4);
    typename stringview_t::size_type j = stringview_t::npos;
    while ((j = shorttext.find(seed, j + 1)) != stringview_t::npos) {
      const strlen_t prefixLength = diff_commonPrefix(traits::mid(longtext, i), traits::mid(shorttext, j));
      const strlen_t suffixLength = diff_commonSuffix(longtext.substr(0, i), shorttext.substr(0, j));
      if (best.mid_common.length() < static_cast<typename stringview_t::size_type>(suffixLength + prefixLength)) {
        best.mid_common = traits::mid(shorttext, j - suffixLength, suffixLength + prefixLength);
        best.text1_a = longtext.substr(0, i - suffixLength);
        best.text1_b = traits::mid(longtext, i + prefixLength);
        best.text2_a = shorttext.substr(0, j - suffixLength);
        best.text2_b = traits::mid(shorttext, j + prefixLength);
      }
    }
    return best.mid_common.length() * 2 >= longtext.length();
  }

  /**
   * Reduce the number of edits by eliminating semantically trivial equalities.
   * @param diffs LinkedList of Diff objects.
   */
 public:
  static void diff_cleanupSemantic(Diffs &diffs) {
    if (diffs.empty()) {
      return;
    }
    bool changes = false;
    std::vector<typename Diffs::iterator> equalities;  // Stack of equalities.
    stringview_t lastequality;  // Always equal to equalities.lastElement().text
    typename Diffs::iterator cur_diff;
    // Number of characters that changed prior to the equality.
    strlen_t length_insertions1 = 0;
    strlen_t length_deletions1 = 0;
    // Number of characters that changed after the equality.
    strlen_t length_insertions2 = 0;
    strlen_t length_deletions2 = 0;

    for (cur_diff = diffs.begin(); cur_diff != diffs.end();) {
      if ((*cur_diff).operation == EQUAL) {
        // Equality found.
        equalities.push_back(cur_diff);
        length_insertions1 = length_insertions2;
        length_deletions1 = length_deletions2;
        length_insertions2 = 0;
        length_deletions2 = 0;
        lastequality = (*cur_diff).text;
      } else {
        // An insertion or deletion.
        if ((*cur_diff).operation == INSERT) {
          length_insertions2 += (*cur_diff).text.length();
        } else {
          length_deletions2 += (*cur_diff).text.length();
        }
        // Eliminate an equality that is smaller or equal to the edits on both
        // sides of it.
        if (!lastequality.empty()
            && (lastequality.length() <= static_cast<typename stringview_t::size_type>(std::max(length_insertions1, length_deletions1)))
            && (lastequality.length() <= static_cast<typename stringview_t::size_type>(std::max(length_insertions2, length_deletions2)))) {
          // printf("Splitting: '%s'\n", qPrintable(lastequality));
          // Walk back to offending equality.
          // Change second copy to insert.
          (*(cur_diff = equalities.back())).operation = INSERT;
          // Duplicate record.
          diffs.insert(cur_diff, Diff(DELETE, lastequality));
          equalities.pop_back();  // Throw away the equality we just deleted.
          if (!equalities.empty()) {
            // Throw away the previous equality (it needs to be reevaluated).
            equalities.pop_back();
          }
          length_insertions1 = 0;  // Reset the counters.
          length_deletions1 = 0;
          length_insertions2 = 0;
          length_deletions2 = 0;
          lastequality = stringview_t();
          changes = true;

          if (!equalities.empty()) {
            // There is a safe equality we can fall back to.
            cur_diff = equalities.back();
		  } else {
            // There are no previous equalities, walk back to the start.
            cur_diff = diffs.begin();
            continue;
          }
        }
      }
      ++cur_diff;
    }

    // Normalize the diff.
    if (changes) {
      diff_cleanupMerge(diffs);
    }
    diff_cleanupSemanticLossless(diffs);

    // Find any overlaps between deletions and insertions.
    // e.g: <del>abcxxx</del><ins>xxxdef</ins>
    //   -> <del>abc</del>xxx<ins>def</ins>
    // e.g: <del>xxxabc</del><ins>defxxx</ins>
    //   -> <ins>def</ins>xxx<del>abc</del>
    // Only extract an overlap if it is as big as the edit ahead or behind it.
    if ((cur_diff = diffs.begin()) != diffs.end()) {
      for (typename Diffs::iterator prev_diff = cur_diff; ++cur_diff != diffs.end(); prev_diff = cur_diff) {
        if ((*prev_diff).operation == DELETE &&
            (*cur_diff).operation == INSERT) {
          stringview_t deletion = (*prev_diff).text;
          stringview_t insertion = (*cur_diff).text;
          strlen_t overlap_length1 = diff_commonOverlap(deletion, insertion);
          strlen_t overlap_length2 = diff_commonOverlap(insertion, deletion);
          if (overlap_length1 >= overlap_length2) {
            if (overlap_length1 >= deletion.size() / 2.0 ||
                overlap_length1 >= insertion.size() / 2.0) {
              // Overlap found.  Insert an equality and trim the surrounding edits.
              diffs.insert(cur_diff, Diff(EQUAL, insertion.substr(0, overlap_length1)));
              prev_diff->text = deletion.substr(0, deletion.length() - overlap_length1);
              cur_diff->text = traits::mid(insertion, overlap_length1);
              // diffs.insert inserts the element before the cursor, so there is
              // no need to step past the new element.
            }
          } else {
            if (overlap_length2 >= deletion.length() / 2.0 ||
                overlap_length2 >= insertion.length() / 2.0) {
              // Reverse overlap found.
              // Insert an equality and swap and trim the surrounding edits.
              diffs.insert(cur_diff, Diff(EQUAL, deletion.substr(0, overlap_length2)));
              prev_diff->operation = INSERT;
              prev_diff->text = insertion.substr(0, insertion.length() - overlap_length2);
              cur_diff->operation = DELETE;
              cur_diff->text = traits::mid(deletion, overlap_length2);
              // diffs.insert inserts the element before the cursor, so there is
              // no need to step past the new element.
            }
          }
          if (++cur_diff == diffs.end()) break;
        }
      }
    }
  }

  /**
   * Look for single edits surrounded on both sides by equalities
   * which can be shifted sideways to align the edit to a word boundary.
   * e.g: The c<ins>at c</ins>ame. -> The <ins>cat </ins>came.
   * @param diffs LinkedList of Diff objects.
   */
 public:
  static void diff_cleanupSemanticLossless(Diffs &diffs)
  {
    // Create a new iterator at the start.
    typename Diffs::iterator prev_diff = diffs.begin(), cur_diff = prev_diff;
    if (prev_diff == diffs.end() || ++cur_diff == diffs.end()) return;

    // construct the strings outside the loop to keep the memory allocations
    string_t equality1, equality2, edit, commonString;
    // Intentionally ignore the first and last element (don't need checking).
    for (typename Diffs::iterator next_diff = cur_diff; ++next_diff != diffs.end(); prev_diff = cur_diff, cur_diff = next_diff) {
      if ((*prev_diff).operation == EQUAL &&
        (*next_diff).operation == EQUAL) {
          // This is a single edit surrounded by equalities.
          equality1 = (*prev_diff).text;
          edit      = (*cur_diff).text;
          equality2 = (*next_diff).text;

          // First, shift the edit as far left as possible.
          strlen_t commonOffset = diff_commonSuffix(equality1, edit);
          if (commonOffset != 0) {
            commonString = traits::mid(edit, edit.length() - commonOffset);
            equality1.erase(equality1.length() - commonOffset);
            edit.erase(edit.length() - commonOffset);
            edit.insert(0, commonString);
            equality2.insert(0, commonString);
          }

          // Second, step character by character right, looking for the best fit.
		  stringview_t bestEquality1 = equality1;
		  stringview_t bestEdit      = edit;
		  stringview_t bestEquality2 = equality2;
          score_t      bestScore = diff_cleanupSemanticScore(equality1, edit)
                                 + diff_cleanupSemanticScore(edit, equality2);
          while (!edit.empty() && !equality2.empty()
              && edit[0] == equality2[0]) {
            equality1 += edit[0];
            edit.erase(0, 1);
            edit += equality2[0];
            equality2 = traits::mid(equality2, 1);
            score_t score = diff_cleanupSemanticScore(equality1, edit)
                + diff_cleanupSemanticScore(edit, equality2);
            // The >= encourages trailing rather than leading whitespace on edits.
            if (score >= bestScore) {
              bestScore = score;
              bestEquality1 = equality1;
              bestEdit = edit;
              bestEquality2 = equality2;
            }
          }

          if ((*prev_diff).text != bestEquality1) {
            // We have an improvement, save it back to the diff.
            if (!bestEquality1.empty()) {
              (*prev_diff).text = bestEquality1;
            } else {
              diffs.erase(prev_diff);
            }
            (*cur_diff).text = bestEdit;
            if (!bestEquality2.empty()) {
              (*next_diff).text = bestEquality2;
            } else {
              diffs.erase(next_diff); // Delete nextDiff.
              next_diff = cur_diff;
              cur_diff = prev_diff;
            }
          }
      }
    }
  }

  /**
   * Given two strings, compute a score representing whether the internal
   * boundary falls on logical boundaries.
   * Scores range from 6 (best) to 0 (worst).
   * @param one First string.
   * @param two Second string.
   * @return The score.
   */
 private:
  static score_t diff_cleanupSemanticScore(const stringview_t &one, const stringview_t &two)
  {
    if (one.empty() || two.empty()) {
      // Edges are the best.
      return 6;
    }

    // Each port of this function behaves slightly differently due to
    // subtle differences in each language's definition of things like
    // 'whitespace'.  Since this function's purpose is largely cosmetic,
    // the choice has been made to use each language's native features
    // rather than force total conformity.
    char_t char1 = one[one.length() - 1];
    char_t char2 = two[0];
    bool nonAlphaNumeric1 = !traits::is_alnum(char1);
    bool nonAlphaNumeric2 = !traits::is_alnum(char2);
    bool whitespace1 = nonAlphaNumeric1 && traits::is_space(char1);
    bool whitespace2 = nonAlphaNumeric2 && traits::is_space(char2);
    bool lineBreak1 = whitespace1 && traits::is_control(char1);
    bool lineBreak2 = whitespace2 && traits::is_control(char2);
    bool blankLine1 = false;
    if (lineBreak1) {
      typename stringview_t::const_reverse_iterator p1 = one.rbegin(), p2 = one.rend();
      if (traits::to_wchar(*p1) == L'\n' && ++p1 != p2) {
        if (traits::to_wchar(*p1) == L'\r')
          ++p1;
        blankLine1 = p1 != p2 && traits::to_wchar(*p1) == L'\n';
      }
    }
    bool blankLine2 = false;
    if (lineBreak2) {
      typename stringview_t::const_iterator p1 = two.end(), p2 = two.begin();
      if (traits::to_wchar(*p2) == L'\r')
        ++p2;
      if (p2 != p1 && traits::to_wchar(*p2) == L'\n') {
        if (++p2 != p1 && traits::to_wchar(*p2) == L'\r')
          ++p2;
        if (p2 != p1 && traits::to_wchar(*p2) == L'\n')
          blankLine2 = true;
      }
    }
  
    if (blankLine1 || blankLine2) {
      // Five points for blank lines.
      return 5;
    } else if (lineBreak1 || lineBreak2) {
      // Four points for line breaks.
      return 4;
    } else if (nonAlphaNumeric1 && !whitespace1 && whitespace2) {
      // Three points for end of sentences.
      return 3;
    } else if (whitespace1 || whitespace2) {
      // Two points for whitespace.
      return 2;
    } else if (nonAlphaNumeric1 || nonAlphaNumeric2) {
      // One point for non-alphanumeric.
      return 1;
    }
    return 0;
  }
  /**
   * Reduce the number of edits by eliminating operationally trivial equalities.
   * @param diffs LinkedList of Diff objects.
   * @param EditCost Cost of an empty edit operation in terms of edit characters, default = 4.
   */
 public:
  static void diff_cleanupEfficiency(Diffs &diffs, uint16_t EditCost = 4)
  {
    if (diffs.empty()) {
      return;
    }
    bool changes = false;
    std::vector<typename Diffs::iterator> equalities;  // Stack of equalities.
    stringview_t lastequality;  // Always equal to equalities.lastElement().text
    // Is there an insertion operation before the last equality.
    bool pre_ins = false;
    // Is there a deletion operation before the last equality.
    bool pre_del = false;
    // Is there an insertion operation after the last equality.
    bool post_ins = false;
    // Is there a deletion operation after the last equality.
    bool post_del = false;

    for (typename Diffs::iterator cur_diff = diffs.begin(); cur_diff != diffs.end();) {
      if ((*cur_diff).operation == EQUAL) {
        // Equality found.
        if ((*cur_diff).text.length() < static_cast<typename string_t::size_type>(EditCost) && (post_ins || post_del)) {
          // Candidate found.
          equalities.push_back(cur_diff);
          pre_ins = post_ins;
          pre_del = post_del;
          lastequality = (*cur_diff).text;
        } else {
          // Not a candidate, and can never become one.
          equalities.clear();
          lastequality = stringview_t();
        }
        post_ins = post_del = false;
      } else {
        // An insertion or deletion.
        if ((*cur_diff).operation == DELETE) {
          post_del = true;
        } else {
          post_ins = true;
        }
        /*
        * Five types to be split:
        * <ins>A</ins><del>B</del>XY<ins>C</ins><del>D</del>
        * <ins>A</ins>X<ins>C</ins><del>D</del>
        * <ins>A</ins><del>B</del>X<ins>C</ins>
        * <ins>A</del>X<ins>C</ins><del>D</del>
        * <ins>A</ins><del>B</del>X<del>C</del>
        */
        if (!lastequality.empty()
            && ((pre_ins && pre_del && post_ins && post_del)
            || ((lastequality.length() < static_cast<typename string_t::size_type>(EditCost / 2))
            && ((pre_ins ? 1 : 0) + (pre_del ? 1 : 0)
            + (post_ins ? 1 : 0) + (post_del ? 1 : 0)) == 3))) {
          // printf("Splitting: '%s'\n", qPrintable(lastequality));
          // Walk back to offending equality.
          // Change second copy to insert.
          (*(cur_diff = equalities.back())).operation = INSERT;
          // Duplicate record.
          diffs.insert(cur_diff, Diff(DELETE, lastequality));
          equalities.pop_back();  // Throw away the equality we just deleted.
          lastequality = stringview_t();
          changes = true;
          if (pre_ins && pre_del) {
            // No changes made which could affect previous entry, keep going.
            post_ins = post_del = true;
            equalities.clear();
          } else {
            if (!equalities.empty()) {
              // Throw away the previous equality (it needs to be reevaluated).
              equalities.pop_back();
            }
            post_ins = post_del = false;
            if (!equalities.empty())
              // There is a safe equality we can fall back to.
              cur_diff = equalities.back();
            else
            {
              // There are no previous equalities, walk back to the start.
              cur_diff = diffs.begin();
              continue;
            }
          }
        }
      }
      ++cur_diff;
    }

    if (changes) {
      diff_cleanupMerge(diffs);
    }
  }

  /**
   * Reorder and merge like edit sections.  Merge equalities.
   * Any edit section can move as long as it doesn't cross an equality.
   * @param diffs LinkedList of Diff objects.
   */
 public:
  static void diff_cleanupMerge(Diffs &diffs)
  {
    diffs.push_back(Diff(EQUAL, string_t()));  // Add a dummy entry at the end.
    typename Diffs::iterator prev_diff, cur_diff;
    int      count_delete = 0;
    int      count_insert = 0;
    string_t text_delete;
    string_t text_insert;
    Diff     *prevEqual = NULL;
    strlen_t commonlength;
    for (cur_diff = diffs.begin(); cur_diff != diffs.end(); ++cur_diff) {
      switch ((*cur_diff).operation) {
        case INSERT:
          count_insert++;
          text_insert += (*cur_diff).text;
          prevEqual = NULL;
          break;
        case DELETE:
          count_delete++;
          text_delete += (*cur_diff).text;
          prevEqual = NULL;
          break;
        case EQUAL:
          if (count_delete + count_insert > 1) {
            // Delete the offending records.
            prev_diff = cur_diff;
            std::advance(prev_diff, -(count_delete + count_insert));
            diffs.erase(prev_diff, cur_diff);
            if (count_delete != 0 && count_insert != 0) {
              // Factor out any common prefixes.
              commonlength = diff_commonPrefix(text_insert, text_delete);
              if (commonlength != 0) {
                if (cur_diff != diffs.begin()) {
                  prev_diff = cur_diff;
                  if ((*--prev_diff).operation != EQUAL) {
                    throw string_t(traits::cs(L"Previous diff should have been an equality."));
                  }
                  (*prev_diff).text += text_insert.substr(0, commonlength);
                } else {
                  diffs.insert(cur_diff, Diff(EQUAL, text_insert.substr(0, commonlength)));
                }
                text_insert = traits::mid(text_insert, commonlength);
                text_delete = traits::mid(text_delete, commonlength);
              }
              // Factor out any common suffixes.
              commonlength = diff_commonSuffix(text_insert, text_delete);
              if (commonlength != 0) {
                (*cur_diff).text = traits::mid(text_insert, text_insert.length() - commonlength);
                (*cur_diff).text += (*cur_diff).text;
                text_insert = text_insert.substr(0, text_insert.length() - commonlength);
                text_delete = text_delete.substr(0, text_delete.length() - commonlength);
              }
            }
            // Insert the merged records.
            if (!text_delete.empty()) {
              diffs.insert(cur_diff, Diff(DELETE, text_delete));
            }
            if (!text_insert.empty()) {
              diffs.insert(cur_diff, Diff(INSERT, text_insert));
            }
          } else if (prevEqual != NULL) {
            // Merge this equality with the previous one.
            prevEqual->text += (*cur_diff).text;
            diffs.erase(cur_diff--);
          }
          
          count_insert = 0;
          count_delete = 0;
          text_delete.clear();
          text_insert.clear();
          prevEqual = &*cur_diff;
          break;
        }

    }
    if (diffs.back().text.empty()) {
      diffs.pop_back();  // Remove the dummy entry at the end.
    }

    /*
    * Second pass: look for single edits surrounded on both sides by equalities
    * which can be shifted sideways to eliminate an equality.
    * e.g: A<ins>BA</ins>C -> <ins>AB</ins>AC
    */
    bool changes = false;
    // Create a new iterator at the start.
    // (As opposed to walking the current one back.)
    prev_diff = cur_diff = diffs.begin();
    if (prev_diff != diffs.end() && ++cur_diff != diffs.end()) {
      // Intentionally ignore the first and last element (don't need checking).
      for (typename Diffs::iterator next_diff = cur_diff; ++next_diff != diffs.end(); prev_diff = cur_diff, cur_diff = next_diff) {
        if ((*prev_diff).operation == EQUAL &&
          (*next_diff).operation == EQUAL) {
            // This is a single edit surrounded by equalities.
            if ((*cur_diff).text.size() >= (*prev_diff).text.size() &&
                (*cur_diff).text.compare((*cur_diff).text.size() - (*prev_diff).text.size(), (*prev_diff).text.size(), (*prev_diff).text) == 0) {
              // Shift the edit over the previous equality.
              (*cur_diff).text = (*prev_diff).text
                  + (*cur_diff).text.substr(0, (*cur_diff).text.length()
                  - (*prev_diff).text.length());
              (*next_diff).text = (*prev_diff).text + (*next_diff).text;
              diffs.erase(prev_diff);
              cur_diff = next_diff;
              changes = true;
              if (++next_diff == diffs.end()) break;
            } else if ((*cur_diff).text.size() >= (*next_diff).text.size() && (*cur_diff).text.compare(0, (*next_diff).text.size(), (*next_diff).text) == 0) {
              // Shift the edit over the next equality.
              (*prev_diff).text += (*next_diff).text;
              (*cur_diff).text = traits::mid((*cur_diff).text, (*next_diff).text.length());
              (*cur_diff).text += (*next_diff).text;
              next_diff = diffs.erase(next_diff); // Delete nextDiff.
              changes = true;
              if (next_diff == diffs.end()) break;
            }
        }
      }
    }
    // If shifts were made, the diff needs reordering and another shift sweep.
    if (changes) {
      diff_cleanupMerge(diffs);
    }
  }

  /**
   * loc is a location in text1, compute and return the equivalent location in
   * text2.
   * e.g. "The cat" vs "The big cat", 1->1, 5->8
   * @param diffs LinkedList of Diff objects.
   * @param loc Location within text1.
   * @return Location within text2.
   */
 public:
  static strlen_t diff_xIndex(const Diffs &diffs, strlen_t loc)
  {
    strlen_t chars1 = 0;
    strlen_t chars2 = 0;
    strlen_t last_chars1 = 0;
    strlen_t last_chars2 = 0;
    typename Diffs::const_iterator last_diff = diffs.end(), cur_diff;
    for (cur_diff = diffs.begin(); cur_diff != diffs.end(); ++cur_diff) {
      if ((*cur_diff).operation != INSERT) {
        // Equality or deletion.
        chars1 += (*cur_diff).text.length();
      }
      if ((*cur_diff).operation != DELETE) {
        // Equality or insertion.
        chars2 += (*cur_diff).text.length();
      }
      if (chars1 > loc) {
        // Overshot the location.
        last_diff = cur_diff;
        break;
      }
      last_chars1 = chars1;
      last_chars2 = chars2;
    }
    if (last_diff != diffs.end() && (*last_diff).operation == DELETE) {
      // The location was deleted.
      return last_chars2;
    }
    // Add the remaining character length.
    return last_chars2 + (loc - last_chars1);
  }

  /**
   * Convert a Diff list into a pretty HTML report.
   * @param diffs LinkedList of Diff objects.
   * @return HTML representation.
   */
 public:
  static string_t diff_prettyHtml(const Diffs &diffs)
  {
    string_t html;
    string_t text;
    for (const auto& cur_diff : diffs)
	{
      typename string_t::size_type n = cur_diff.text.size();
      typename string_t::const_pointer p, end;
      for (p = cur_diff.text.data(), end = p + n; p != end; ++p)
        switch (traits::to_wchar(*p)) {
          case L'&':  n += 4; break;
          case L'<':
          case L'>':  n += 3; break;
          case L'\n': n += 9; break;
        }
      if (n == cur_diff.text.size())
        text = cur_diff.text;
      else {
        text.clear();
        text.reserve(n);
        for (p = cur_diff.text.data(); p != end; ++p)
          switch (traits::to_wchar(*p)) {
            case L'&':  text += traits::cs(L"&amp;");      break;
            case L'<':  text += traits::cs(L"&lt;");       break;
            case L'>':  text += traits::cs(L"&gt;");       break;
            case L'\n': text += traits::cs(L"&para;<br>"); break;
            default:    text += *p;                        break;
          }
      }
      switch (cur_diff.operation) {
        case INSERT:
          html += traits::cs(L"<ins style=\"background:#e6ffe6;\">");
          html += text;
          html += traits::cs(L"</ins>");
          break;
        case DELETE:
          html += traits::cs(L"<del style=\"background:#ffe6e6;\">");
          html += text;
          html += traits::cs(L"</del>");
          break;
        case EQUAL:
          html += traits::cs(L"<span>");
          html += text;
          html += traits::cs(L"</span>");
          break;
      }
    }
    return html;
  }

  /**
   * Compute and return the source text (all equalities and deletions).
   * @param diffs LinkedList of Diff objects.
   * @return Source text.
   */
 public:
  static string_t diff_text1(const Diffs &diffs)
  {
    string_t text;
    for (const auto& cur_diff : diffs)
	{
      if (cur_diff.operation != INSERT) {
        text += cur_diff.text;
      }
    }
    return text;
  }

  /**
   * Compute and return the destination text (all equalities and insertions).
   * @param diffs LinkedList of Diff objects.
   * @return Destination text.
   */
 public:
  static string_t diff_text2(const Diffs &diffs)
  {
    string_t text;
    for (const auto& cur_diff : diffs)
	{
      if (cur_diff.operation != DELETE) {
        text += cur_diff.text;
      }
    }
    return text;
  }

  /**
   * Compute the Levenshtein distance; the number of inserted, deleted or
   * substituted characters.
   * @param diffs LinkedList of Diff objects.
   * @return Number of changes.
   */
 public:
  static strlen_t diff_levenshtein(const Diffs &diffs)
  {
    strlen_t levenshtein = 0;
    strlen_t insertions = 0;
    strlen_t deletions = 0;
    for (const auto& cur_diff : diffs)
	{
      switch (cur_diff.operation) {
        case INSERT:
          insertions += cur_diff.text.length();
          break;
        case DELETE:
          deletions += cur_diff.text.length();
          break;
        case EQUAL:
          // A deletion and an insertion is one substitution.
          levenshtein += std::max(insertions, deletions);
          insertions = 0;
          deletions = 0;
          break;
      }
    }
    levenshtein += std::max(insertions, deletions);
    return levenshtein;
  }

  /**
   * Crush the diff into an encoded string which describes the operations
   * required to transform text1 into text2.
   * E.g. =3\t-2\t+ing  -> Keep 3 chars, delete 2 chars, insert 'ing'.
   * Operations are tab-separated.  Inserted text is escaped using %xx notation.
   * @param diffs Array of diff tuples.
   * @return Delta text.
   */
 public:
  static string_t diff_toDelta(const Diffs &diffs)
  {
    string_t text;
    for (const auto& cur_diff : diffs)
	{
      switch (cur_diff.operation) {
        case INSERT: {
          text += traits::from_wchar(L'+');
          append_percent_encoded(text, cur_diff.text);
          text += traits::from_wchar(L'\t');
          break;
        }
        case DELETE:
          text += traits::from_wchar(L'-');
          text += to_string(cur_diff.text.length());
          text += traits::from_wchar(L'\t');
          break;
        case EQUAL:
          text += traits::from_wchar(L'=');
          text += to_string(cur_diff.text.length());
          text += traits::from_wchar(L'\t');
          break;
      }
    }
    if (!text.empty()) {
      // Strip off trailing tab character.
      text.erase(text.length() - 1, 1);
    }
    return text;
  }

  /**
   * Given the original text1, and an encoded string which describes the
   * operations required to transform text1 into text2, compute the full diff.
   * @param text1 Source string for the diff.
   * @param delta Delta text.
   * @return Array of diff tuples or null if invalid.
   * @throws string_t If invalid input.
   */
 public:
  static Diffs diff_fromDelta(const stringview_t &text1, const stringview_t &delta)
  {
    Diffs diffs;
    strlen_t pointer = 0;  // Cursor in text1
    typename stringview_t::size_type token_len;
    for (typename stringview_t::const_pointer token = delta.data(); token - delta.data() < delta.length(); token += token_len + 1) {
      token_len = next_token(delta, traits::from_wchar(L'\t'), token);
      if (token_len == 0) {
        // Blank tokens are ok (from a trailing \t).
        continue;
      }
      // Each token begins with a one character parameter which specifies the
      // operation of this token (delete, insert, equality).
      string_t param(token + 1, token_len - 1);
      switch (traits::to_wchar(*token)) {
        case L'+':
          percent_decode(param);
          diffs.push_back(Diff(INSERT, param));
          break;
        case L'-':
          // Fall through.
        case L'=': {
          auto n = traits::to_uint32(param);
          if (n < 0) {
            throw string_t(traits::cs(L"Negative number in diff_fromDelta: ") + param);
          }
          stringview_t text = traits::mid(text1, pointer, n);
          pointer += n;
          if (traits::to_wchar(*token) == L'=') {
            diffs.push_back(Diff(EQUAL, text));
          } else {
            diffs.push_back(Diff(DELETE, text));
          }
          break;
        }
        default:
          throw string_t(string_t(traits::cs(L"Invalid diff operation in diff_fromDelta: ")) + *token);
      }
    }
    if (pointer != text1.length()) {
      throw string_t(traits::cs(L"Delta length (") + to_string(pointer)
                     + traits::cs(L") smaller than source text length (")
                     + to_string(text1.length()) + traits::from_wchar(L')'));
    }
    return diffs;
  }


  //  MATCH FUNCTIONS


  /**
   * Locate the best instance of 'pattern' in 'text' near 'loc'.
   * Returns npos if no match found.
   * @param text The text to search.
   * @param pattern The pattern to search for.
   * @param loc The location to search around.
   * @return Best match index or npos.
   */
 public:
  strlen_t match_main(const stringview_t &text, const stringview_t &pattern, strlen_t loc) const
  {
    loc = std::min(loc, text.length());
    if (text == pattern) {
      // Shortcut (potentially not guaranteed by the algorithm)
      return 0;
    } else if (text.empty()) {
      // Nothing to match.
      return npos;
    } else if (loc + pattern.length() <= text.length()
        && traits::mid(text, loc, pattern.length()) == pattern) {
      // Perfect match at the perfect spot!  (Includes case of null pattern)
      return loc;
    } else {
      // Do a fuzzy compare.
      return match_bitap(text, pattern, loc);
    }
  }

  /**
   * Locate the best instance of 'pattern' in 'text' near 'loc' using the
   * Bitap algorithm.  Returns npos if no match found.
   * @param text The text to search.
   * @param pattern The pattern to search for.
   * @param loc The location to search around.
   * @return Best match index or npos.
   */
 protected:
  strlen_t match_bitap(const stringview_t &text, const stringview_t &pattern, strlen_t loc) const
  {
    if (!(Match_MaxBits == 0 || pattern.length() <= Match_MaxBits)) {
      throw string_t(traits::cs(L"Pattern too long for this application."));
    }

    // Initialise the alphabet.
    std::map<char_t, step_t> s;
    match_alphabet(pattern, s);

    // Highest score beyond which we give up.
    double score_threshold = Match_Threshold;
    // Is there a nearby exact match? (speedup)
    typename stringview_t::size_type best_loc = text.find(pattern, loc);
    if (best_loc != stringview_t::npos) {
      score_threshold = std::min(match_bitapScore(0, best_loc, loc, pattern), score_threshold);
      // What about in the other direction? (speedup)
      best_loc = text.rfind(pattern, loc + pattern.length());
      if (best_loc != stringview_t::npos) {
        score_threshold = std::min(match_bitapScore(0, best_loc, loc, pattern), score_threshold);
      }
    }

    // Initialise the bit arrays.
    step_t matchmask = 1 << (pattern.length() - 1);
    best_loc = stringview_t::npos;

    strlen_t bin_min, bin_mid;
    strlen_t bin_max = pattern.length() + text.length();
    step_t *rd;
    step_t *last_rd = nullptr;
    for (step_t d = 0; d < pattern.length(); d++) {
      // Scan for the best match; each iteration allows for one more error.
      // Run a binary search to determine how far from 'loc' we can stray at
      // this error level.
      bin_min = 0;
      bin_mid = bin_max;
      while (bin_min < bin_mid) {
        if (match_bitapScore(d, loc + bin_mid, loc, pattern) <= score_threshold) {
          bin_min = bin_mid;
        } else {
          bin_max = bin_mid;
        }
        bin_mid = (bin_max - bin_min) / 2 + bin_min;
      }
      // Use the result from this iteration as the maximum for the next.
      bin_max = bin_mid;
      strlen_t start  = (bin_mid > loc) ? 1 : loc - bin_mid + 1;
      strlen_t finish = std::min(loc + bin_mid, text.length()) + pattern.length();

      rd = new step_t[finish + 2];
      rd[finish + 1] = (1 << d) - 1;
      for (strlen_t j = finish; j >= start; j--) {
        step_t charMatch;
        if (text.length() <= j - 1) {
          // Out of range.
          charMatch = 0;
        } else {
          charMatch = s[text[j - 1]];
        }
        if (d == 0) {
          // First pass: exact match.
          rd[j] = ((rd[j + 1] << 1) | 1) & charMatch;
        } else {
          // Subsequent passes: fuzzy match.
          rd[j] = (((rd[j + 1] << 1) | 1) & charMatch)
              | (((last_rd[j + 1] | last_rd[j]) << 1) | 1)
              | last_rd[j + 1];
        }
        if ((rd[j] & matchmask) != 0) {
          double score = match_bitapScore(d, j - 1, loc, pattern);
          // This match will almost certainly be better than any existing
          // match.  But check anyway.
          if (score <= score_threshold) {
            // Told you so.
            score_threshold = score;
            best_loc = j - 1;
            if (best_loc > loc) {
              // When passing loc, don't exceed our current distance from loc.
              start = (best_loc >= 2 * loc) ? 1 : 2 * loc - best_loc;
            } else {
              // Already passed loc, downhill from here on in.
              break;
            }
          }
        }
      }
      if (match_bitapScore(d + 1, loc, loc, pattern) > score_threshold) {
        // No hope for a (better) match at greater error levels.
        break;
      }
      delete [] last_rd;
      last_rd = rd;
    }
    delete [] last_rd;
    delete [] rd;
    return best_loc;
  }

  /**
   * Compute and return the score for a match with e errors and x location.
   * @param e Number of errors in match.
   * @param x Location of match.
   * @param loc Expected location of match.
   * @param pattern Pattern being sought.
   * @return Overall score for match (0.0 = good, 1.0 = bad).
   */
 private:
  double match_bitapScore(step_t e, strlen_t x, strlen_t loc, const stringview_t &pattern) const
  {
    const float accuracy = static_cast<float> (e) / pattern.length();
    const strlen_t proximity = (loc - x < 0)? (x - loc) : (loc - x);
    if (Match_Distance == 0) {
      // Dodge divide by zero error.
      return proximity == 0 ? accuracy : 1.0;
    }
    return accuracy + (proximity / static_cast<float> (Match_Distance));
  }

  /**
   * Initialise the alphabet for the Bitap algorithm.
   * @param pattern The text to encode.
   * @return Hash of character locations.
   */
 protected:
  static void match_alphabet(const stringview_t &pattern, std::map<char_t, step_t>& s)
  {
    // There is no need to initialize map values, since they are zero-initialized by default
    for (typename stringview_t::size_type i = 0; i < pattern.length(); i++)
      s[pattern[i]] |= (1 << (pattern.length() - i - 1));
  }


 //  PATCH FUNCTIONS


  /**
   * Increase the context until it is unique,
   * but don't let the pattern expand beyond Match_MaxBits.
   * @param patch The patch to grow.
   * @param text Source text.
   */
 protected:
  void patch_addContext(Patch &patch, const stringview_t &text) const
  {
    if (text.empty()) {
      return;
    }
    stringview_t pattern = traits::mid(text, patch.start2, patch.length1);
    padding_t padding = 0;

    // Look for the first and last matches of pattern in text.  If two different
    // matches are found, increase the pattern length.
    while (text.find(pattern) != text.rfind(pattern) && pattern.length() < MaxMatchPattern) {
      padding += Patch_Margin;
      strlen_t iOffset = (padding > patch.start2) ? 0 : patch.start2 - padding;
      pattern = traits::mid(text, iOffset,
          std::min(text.length(), static_cast<typename stringview_t::size_type>(patch.start2 + patch.length1 + padding))
          - iOffset);
    }
    // Add one chunk for good luck.
    padding += Patch_Margin;

    // Add the prefix.
    strlen_t iOffset = (padding > patch.start2) ? 0 : patch.start2 - padding;
    stringview_t prefix = traits::mid(text, iOffset, patch.start2 - iOffset);
    if (!prefix.empty()) {
      patch.diffs.push_front(Diff(EQUAL, prefix));
    }
    // Add the suffix.
    stringview_t suffix = traits::mid(text, patch.start2 + patch.length1,
        std::min(text.length(), static_cast<typename stringview_t::size_type>(patch.start2 + patch.length1 + padding))
        - (patch.start2 + patch.length1));
    if (!suffix.empty()) {
      patch.diffs.push_back(Diff(EQUAL, suffix));
    }

    // Roll back the start points.
    patch.start1 -= prefix.length();
    patch.start2 -= prefix.length();
    // Extend the lengths.
    patch.length1 += prefix.length() + suffix.length();
    patch.length2 += prefix.length() + suffix.length();
  }

  /**
   * Compute a list of patches to turn text1 into text2.
   * A set of diffs will be computed.
   * @param text1 Old text.
   * @param text2 New text.
   * @return LinkedList of Patch objects.
   */
 public:
  Patches patch_make(const stringview_t &text1, const stringview_t &text2) const
  {
    // No diffs provided, compute our own.
    Diffs diffs = diff_main(text1, text2, true);
    if (diffs.size() > 2) {
      diff_cleanupSemantic(diffs);
      diff_cleanupEfficiency(diffs);
    }

    return patch_make(text1, diffs);
  }

  /**
   * Compute a list of patches to turn text1 into text2.
   * text1 will be derived from the provided diffs.
   * @param diffs Array of diff tuples for text1 to text2.
   * @return LinkedList of Patch objects.
   */
 public:
  Patches patch_make(const Diffs &diffs) const
  {
    // No origin string provided, compute our own.
    return patch_make(diff_text1(diffs), diffs);
  }

  /**
   * Compute a list of patches to turn text1 into text2.
   * text2 is ignored, diffs are the delta between text1 and text2.
   * @param text1 Old text.
   * @param text2 Ignored.
   * @param diffs Array of diff tuples for text1 to text2.
   * @return LinkedList of Patch objects.
   * @deprecated Prefer patch_make(const string_t &text1, const Diffs &diffs).
   */
 public:
  Patches patch_make(const stringview_t &text1, const stringview_t &/*text2*/, const Diffs &diffs) const
  {
    return patch_make(text1, diffs); // text2 is entirely unused.
  }

  /**
   * Compute a list of patches to turn text1 into text2.
   * text2 is not provided, diffs are the delta between text1 and text2.
   * @param text1 Old text.
   * @param diffs Array of diff tuples for text1 to text2.
   * @return LinkedList of Patch objects.
   */
 public:
  Patches patch_make(const stringview_t &text1, const Diffs &diffs) const
  {
    Patches patches;
    if (!diffs.empty()) { // Get rid of the null case.
      Patch patch;
      strlen_t char_count1 = 0;  // Number of characters into the text1 string.
      strlen_t char_count2 = 0;  // Number of characters into the text2 string.
      // Start with text1 (prepatch_text) and apply the diffs until we arrive at
      // text2 (postpatch_text).  We recreate the patches one by one to determine
      // context info.
      string_t prepatch_text(text1);
      string_t postpatch_text(prepatch_text);
      for (const auto& cur_diff : diffs)
	  {
        if (patch.diffs.empty() && cur_diff.operation != EQUAL) {
          // A new patch starts here.
          patch.start1 = char_count1;
          patch.start2 = char_count2;
        }

        switch (cur_diff.operation) {
          case INSERT:
            patch.diffs.push_back(cur_diff);
            patch.length2 += cur_diff.text.length();
            postpatch_text = postpatch_text.substr(0, char_count2);
            postpatch_text += cur_diff.text;
            postpatch_text += traits::mid(postpatch_text, char_count2);
            break;
          case DELETE:
            patch.length1 += cur_diff.text.length();
            patch.diffs.push_back(cur_diff);
            postpatch_text = postpatch_text.substr(0, char_count2);
            postpatch_text += traits::mid(postpatch_text, char_count2 + cur_diff.text.length());
            break;
          case EQUAL:
            if (cur_diff.text.length() <= 2 * Patch_Margin
                && !patch.diffs.empty() && !(cur_diff == diffs.back())) {
              // Small equality inside a patch.
              patch.diffs.push_back(cur_diff);
              patch.length1 += cur_diff.text.length();
              patch.length2 += cur_diff.text.length();
            }

            if (cur_diff.text.length() >= 2 * Patch_Margin) {
              // Time for a new patch.
              if (!patch.diffs.empty()) {
                patch_addContext(patch, prepatch_text);
                patches.push_back(patch);
                patch = Patch();
                // Unlike Unidiff, our patch lists have a rolling context.
                // http://code.google.com/p/google-diff-match-patch/wiki/Unidiff
                // Update prepatch text & pos to reflect the application of the
                // just completed patch.
                prepatch_text = postpatch_text;
                char_count1 = char_count2;
              }
            }
            break;
        }

        // Update the current character count.
        if (cur_diff.operation != INSERT) {
          char_count1 += cur_diff.text.length();
        }
        if (cur_diff.operation != DELETE) {
          char_count2 += cur_diff.text.length();
        }
      }
      // Pick up the leftover patch if not empty.
      if (!patch.diffs.empty()) {
        patch_addContext(patch, prepatch_text);
        patches.push_back(patch);
      }
    }
    return patches;
  }

public:
  /**
   * Merge a set of patches onto the text.  Return a patched text, as well
   * as an array of true/false values indicating which patches were applied.
   * @param patches Array of patch objects.
   * @param text Old text.
   * @return Two element Object array, containing the new text and an array of
   *      boolean values.
   */
  std::pair<string_t, std::vector<bool>> patch_apply(const Patches &patches, const stringview_t &sourceText) const
  {
    std::pair<string_t, std::vector<bool>> res;

    if (patches.empty()) {
      res.first = sourceText;
      res.second.clear();
      return res;
    }
    string_t text = sourceText;  // Copy to preserve original.

    // Deep copy the patches so that no changes are made to originals.
    Patches patchesCopy(patches); // Default copy constructor will do it just fine

    string_t nullPadding = patch_addPadding(patchesCopy);
    text = nullPadding + text + nullPadding;
    patch_splitMax(patchesCopy);

    // delta keeps track of the offset between the expected and actual location
    // of the previous patch.  If there are patches expected at positions 10 and
    // 20, but the first patch was found at 12, delta is 2 and the second patch
    // has an effective expected position of 22.
    step_t delta = 0;
    std::size_t x = 0;
    auto& results = res.second;
    results.resize(patchesCopy.size());
    string_t text1, text2;
    for (const auto& cur_patch : patchesCopy)
	{
      strlen_t expected_loc = cur_patch.start2 + delta;
      text1 = diff_text1(cur_patch.diffs);
      strlen_t start_loc;
      strlen_t end_loc = npos;
      if (text1.length() > Match_MaxBits) {
        // patch_splitMax will only provide an oversized pattern in the case of
        // a monster delete.
        start_loc = match_main(text, text1.substr(0, Match_MaxBits), expected_loc);
        if (start_loc != npos) {
          end_loc = match_main(text, traits::right(text1, Match_MaxBits),
              expected_loc + text1.length() - Match_MaxBits);
          if (end_loc == npos || start_loc >= end_loc) {
            // Can't find valid trailing context.  Drop this patch.
            start_loc = npos;
          }
        }
      } else {
        start_loc = match_main(text, text1, expected_loc);
      }
      if (start_loc == npos) {
        // No match found.  :(
        results[x] = false;
        // Subtract the delta for this failed patch from subsequent patches.
        delta -= cur_patch.length2 - cur_patch.length1;
      } else {
        // Found a match.  :)
        results[x] = true;
        delta = start_loc - expected_loc;
        if (end_loc == npos) {
          text2 = traits::mid(text, start_loc, text1.length());
        } else {
          text2 = traits::mid(text, start_loc, end_loc + Match_MaxBits - start_loc);
        }
        if (text1 == text2) {
          // Perfect match, just shove the replacement text in.
          text = text.substr(0, start_loc) + diff_text2(cur_patch.diffs) + traits::mid(text, start_loc + text1.length());
        } else {
          // Imperfect match.  Run a diff to get a framework of equivalent
          // indices.
          Diffs diffs = diff_main(text1, text2, false);
          if (text1.length() > Match_MaxBits
              && diff_levenshtein(diffs) / static_cast<float> (text1.length())
              > Patch_DeleteThreshold) {
            // The end points match, but the content is unacceptably bad.
            results[x] = false;
          } else {
            diff_cleanupSemanticLossless(diffs);
            strlen_t index1 = 0;
            for (const auto& cur_diff : cur_patch.diffs)
			{
              if (cur_diff.operation != EQUAL) {
                strlen_t index2 = diff_xIndex(diffs, index1);
                if (cur_diff.operation == INSERT) {
                  // Insertion
                  text = text.substr(0, start_loc + index2) + (*cur_diff).text + traits::mid(text, start_loc + index2);
                } else if (cur_diff.operation == DELETE) {
                  // Deletion
                  text = text.substr(0, start_loc + index2) + traits::mid(text, start_loc + diff_xIndex(diffs, index1 + (*cur_diff).text.length()));
                }
              }
              if (cur_diff.operation != DELETE) {
                index1 += cur_diff.text.length();
              }
            }
          }
        }
      }
      x++;
    }
    // Strip the padding off.
    res.first = traits::mid(text, nullPadding.length(), text.length() - 2 * nullPadding.length());

    return res;
  }

  /**
   * Add some padding on text start and end so that edges can match something.
   * Intended to be called only from within patch_apply.
   * @param patches Array of patch objects.
   * @return The padding string added to each side.
   */
 private:
  string_t patch_addPadding(Patches &patches) const
  {
    padding_t paddingLength = Patch_Margin;
    string_t nullPadding;
    for (padding_t x = 1; x <= paddingLength; x++) {
      nullPadding += (char_t)x;
    }

    // Bump all the patches forward.
    for (auto& cur_patch : patches)
	{
      cur_patch.start1 += paddingLength;
      cur_patch.start2 += paddingLength;
    }

    // Add some padding on start of first diff.
    Patch &firstPatch = patches.front();
    Diffs &firstPatchDiffs = firstPatch.diffs;
    if (firstPatchDiffs.empty() || firstPatchDiffs.front().operation != EQUAL) {
      // Add nullPadding equality.
      firstPatchDiffs.push_front(Diff(EQUAL, nullPadding));
      firstPatch.start1 -= paddingLength;  // Should be 0.
      firstPatch.start2 -= paddingLength;  // Should be 0.
      firstPatch.length1 += paddingLength;
      firstPatch.length2 += paddingLength;
    } else if (paddingLength > firstPatchDiffs.front().text.length()) {
      // Grow first equality.
      Diff &firstDiff = firstPatchDiffs.front();
      strlen_t extraLength = paddingLength - firstDiff.text.length();
      firstDiff.text = traits::mid(nullPadding, firstDiff.text.length(),
          paddingLength - firstDiff.text.length()) + firstDiff.text;
      firstPatch.start1 -= extraLength;
      firstPatch.start2 -= extraLength;
      firstPatch.length1 += extraLength;
      firstPatch.length2 += extraLength;
    }

    // Add some padding on end of last diff.
    Patch &lastPatch = patches.front();
    Diffs &lastPatchDiffs = lastPatch.diffs;
    if (lastPatchDiffs.empty() || lastPatchDiffs.back().operation != EQUAL) {
      // Add nullPadding equality.
      lastPatchDiffs.push_back(Diff(EQUAL, nullPadding));
      lastPatch.length1 += paddingLength;
      lastPatch.length2 += paddingLength;
    } else if (paddingLength > lastPatchDiffs.back().text.length()) {
      // Grow last equality.
      Diff &lastDiff = lastPatchDiffs.back();
      strlen_t extraLength = paddingLength - lastDiff.text.length();
      lastDiff.text += nullPadding.substr(0, extraLength);
      lastPatch.length1 += extraLength;
      lastPatch.length2 += extraLength;
    }

    return nullPadding;
  }

  /**
   * Look through the patches and break up any which are longer than the
   * maximum limit of the match algorithm.
   * Intended to be called only from within patch_apply.
   * @param patches LinkedList of Patch objects.
   */
 private:
  void patch_splitMax(Patches &patches) const
  {
    string_t precontext, postcontext;
    Patch patch;
    strlen_t start1, start2;
    bool empty;
    Operation diff_type;
    string_t diff_text;
    Patch bigpatch;

    for (typename Patches::iterator cur_patch = patches.begin(); cur_patch != patches.end();) {
      if ((*cur_patch).length1 <= Match_MaxBits) { ++cur_patch; continue; }
      bigpatch = *cur_patch;
      // Remove the big old patch.
      cur_patch = patches.erase(cur_patch);
      start1 = bigpatch.start1;
      start2 = bigpatch.start2;
      precontext.clear();
      while (!bigpatch.diffs.empty()) {
        // Create one of several smaller patches.
        patch = Patch();
        empty = true;
        patch.start1 = start1 - precontext.length();
        patch.start2 = start2 - precontext.length();
        if (!precontext.empty()) {
          patch.length1 = patch.length2 = precontext.length();
          patch.diffs.push_back(Diff(EQUAL, precontext));
        }
        while (!bigpatch.diffs.empty()
            && patch.length1 < Match_MaxBits - Patch_Margin) {
          diff_type = bigpatch.diffs.front().operation;
          diff_text = bigpatch.diffs.front().text;
          if (diff_type == INSERT) {
            // Insertions are harmless.
            patch.length2 += diff_text.length();
            start2 += diff_text.length();
            patch.diffs.push_back(bigpatch.diffs.front());
            bigpatch.diffs.pop_front();
            empty = false;
          } else if (diff_type == DELETE && patch.diffs.size() == 1
              && patch.diffs.front().operation == EQUAL
              && diff_text.length() > 2 * Match_MaxBits) {
            // This is a large deletion.  Let it pass in one chunk.
            patch.length1 += diff_text.length();
            start1 += diff_text.length();
            empty = false;
            patch.diffs.push_back(Diff(diff_type, diff_text));
            bigpatch.diffs.pop_front();
          } else {
            // Deletion or equality.  Only take as much as we can stomach.
            diff_text = diff_text.substr(0, std::min(diff_text.length(), Match_MaxBits - patch.length1 - Patch_Margin));
            patch.length1 += diff_text.length();
            start1 += diff_text.length();
            if (diff_type == EQUAL) {
              patch.length2 += diff_text.length();
              start2 += diff_text.length();
            } else {
              empty = false;
            }
            patch.diffs.push_back(Diff(diff_type, diff_text));
            if (diff_text == bigpatch.diffs.front().text) {
              bigpatch.diffs.pop_front();
            } else {
              bigpatch.diffs.front().text = traits::mid(bigpatch.diffs.front().text, diff_text.length());
            }
          }
        }
        // Compute the head context for the next patch.
        precontext = traits::mid(diff_text2(patch.diffs), (Patch_Margin > precontext.length()) ? 0 : precontext.length - Patch_Margin);
        // Append the end context for this patch.
        postcontext = diff_text1(bigpatch.diffs);
        if (postcontext.length() > Patch_Margin) {
          postcontext = postcontext.substr(0, Patch_Margin);
        }
        if (!postcontext.empty()) {
          patch.length1 += postcontext.length();
          patch.length2 += postcontext.length();
          if (!patch.diffs.empty()
              && patch.diffs.back().operation == EQUAL) {
            patch.diffs.back().text += postcontext;
          } else {
            patch.diffs.push_back(Diff(EQUAL, postcontext));
          }
        }
        if (!empty) {
          patches.insert(cur_patch, patch);
        }
      }
    }
  }

  /**
   * Take a list of patches and return a textual representation.
   * @param patches List of Patch objects.
   * @return Text representation of patches.
   */
 public:
  static string_t patch_toText(const Patches &patches) {
    string_t text;
    for (const auto& cur_patch : patches)
	{
      text += cur_patch.toString();
    }
    return text;
  }

  /**
   * Parse a textual representation of patches and return a List of Patch
   * objects.
   * @param textline Text representation of patches.
   * @return List of Patch objects.
   * @throws string_t If invalid input.
   */
 public:
  Patches patch_fromText(const stringview_t &textline) const {
    Patches patches;
    if (!textline.empty()) {
      char_t sign;
      string_t line;
      typename string_t::const_pointer text = textline.data();
      typename string_t::size_type text_len, l;
      while (text - textline.data() < textline.length()) {
        if ((text_len = next_token(textline, traits::from_wchar(L'\n'), text)) == 0) { ++text; continue; }

        // A replacement for the regexp "^@@ -(\\d+),?(\\d*) \\+(\\d+),?(\\d*) @@$" exact match
        string_t start1, length1, start2, length2;
        do {
          typename string_t::const_pointer t = text;
          l = text_len;
          if ((l -= 9) > 0 && traits::to_wchar(*t) == L'@' && traits::to_wchar(*++t) == L'@'
               && traits::to_wchar(*++t) == L' ' && traits::to_wchar(*++t) == L'-' && traits::is_digit(*++t)) {
            do { start1 += *t; } while (--l > 0 && traits::is_digit(*++t));
            if (l > 0 && traits::to_wchar(*t) == L',') ++t, --l;
            while (l > 0 && traits::is_digit(*t)) --l, length1 += *t++;
            if (l > 0 && traits::to_wchar(*t++) == L' ' && traits::to_wchar(*t++) == L'+' && traits::is_digit(*t)) {
              do { start2 += *t; --l; } while (traits::is_digit(*++t));
              if (l > 0 && traits::to_wchar(*t) == L',') ++t, --l;
              while (l > 0 && traits::is_digit(*t)) --l, length2 += *t++;
              if (l == 0 && traits::to_wchar(*t++) == L' ' && traits::to_wchar(*t++) == L'@' && traits::to_wchar(*t) == L'@') break; // Success
            }
          }
          throw string_t(traits::cs(L"Invalid patch string: ") + string_t(text, text_len));
        } while (false);

        Patch patch;
        patch.start1 = traits::to_uint32(start1);
        if (length1.empty()) {
          patch.start1--;
          patch.length1 = 1;
        } else if (length1.size() == 1 && traits::to_wchar(length1[0]) == L'0') {
          patch.length1 = 0;
        } else {
          patch.start1--;
          patch.length1 = traits::to_uint32(length1);
        }

        patch.start2 = traits::to_uint32(start2);
        if (length2.empty()) {
          patch.start2--;
          patch.length2 = 1;
        } else if (length2.size() == 1 && traits::to_wchar(length2[0]) == L'0') {
          patch.length2 = 0;
        } else {
          patch.start2--;
          patch.length2 = traits::to_uint32(length2);
        }

        for (text += text_len + 1; text - textline.data() < textline.length(); text += text_len + 1) {
          if ((text_len = next_token(textline, traits::from_wchar(L'\n'), text)) == 0) continue;

          sign = *text;
          line.assign(text + 1, text_len - 1);
          percent_decode(line);
          switch (traits::to_wchar(sign)) {
            case L'-':
              // Deletion.
              patch.diffs.push_back(Diff(DELETE, line));
              continue;
            case L'+':
              // Insertion.
              patch.diffs.push_back(Diff(INSERT, line));
              continue;
            case L' ':
              // Minor equality.
              patch.diffs.push_back(Diff(EQUAL, line));
              continue;
            case L'@':
              // Start of next patch.
              break;
            default:
              // WTF?
              throw string_t(traits::cs(L"Invalid patch mode '") + (sign + (traits::cs(L"' in: ") + line)));
          }
          break;
        }

        patches.push_back(patch);
      }
    }
    return patches;
  }

  /**
   * Utility functions
   */
 private:

  static typename string_t::size_type next_token(const string_t& str, char_t delim, typename string_t::const_pointer off)
  {
    typename string_t::const_pointer p = off, end = str.data() + str.length();
    for (; p != end; ++p) if (*p == delim) break;
    return p - off;
  }

  static void append_percent_encoded(string_t& s1, const stringview_t& s2)
  {
    const wchar_t safe_chars[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~ !*'();/?:@&=+$,#";

    std::size_t safe[0x100], i;
    for (i = 0; i < 0x100; ++i) safe[i] = 0;
    for (i = 0; i < sizeof(safe_chars) / sizeof(wchar_t); ++i) safe[safe_chars[i]] = i + 1;

    int n = 0;
    typename traits::utf32_t u;
    typename string_t::const_pointer c = s2.data(), end = c + s2.length();
    while (c != end) {
      c = traits::to_utf32(c, end, u);
      n += u >= 0x10000? 12 : u >= 0x800? 9 : u >= 0x80? 6 : safe[static_cast<unsigned char>(u)]? 1 : 3;
    }
    if (n == int(s2.length()))
      s1.append(s2);
    else {
      s1.reserve(s1.size() + n);
      // Encode as UTF-8, then escape unsafe characters
      unsigned char utf8[4];
      for (c = s2.data(); c != end;) {
        c = traits::to_utf32(c, end, u);
        unsigned char* pt = utf8;
        if (u < 0x80)
          *pt++ = (unsigned char)u;  
        else if (u < 0x800) {
          *pt++ = (unsigned char)((u >> 6) | 0xC0);
          *pt++ = (unsigned char)((u & 0x3F) | 0x80);
        }
        else if (u < 0x10000) {
          *pt++ = (unsigned char)((u >> 12) | 0xE0);
          *pt++ = (unsigned char)(((u >> 6) & 0x3F) | 0x80);
          *pt++ = (unsigned char)((u & 0x3F) | 0x80);
        }
        else {
          *pt++ = (unsigned char)((u >> 18) | 0xF0);
          *pt++ = (unsigned char)(((u >> 12) & 0x3F) | 0x80);
          *pt++ = (unsigned char)(((u >> 6) & 0x3F) | 0x80);
          *pt++ = (unsigned char)((u & 0x3F) | 0x80);
        }

        for (const unsigned char* p = utf8; p < pt; ++p)
          if (safe[*p])
            s1 += traits::from_wchar(safe_chars[safe[*p] - 1]);
          else {
            s1 += traits::from_wchar(L'%');
            s1 += traits::from_wchar(safe_chars[(*p & 0xF0) >> 4]);
            s1 += traits::from_wchar(safe_chars[*p & 0xF]);
          }
      }
    }
  }

  static unsigned hex_digit_value(char_t c)
  {
    switch (traits::to_wchar(c))
    {
      case L'0': return 0;
      case L'1': return 1;
      case L'2': return 2;
      case L'3': return 3;
      case L'4': return 4;
      case L'5': return 5;
      case L'6': return 6;
      case L'7': return 7;
      case L'8': return 8;
      case L'9': return 9;
      case L'A': case L'a': return 0xA;
      case L'B': case L'b': return 0xB;
      case L'C': case L'c': return 0xC;
      case L'D': case L'd': return 0xD;
      case L'E': case L'e': return 0xE;
      case L'F': case L'f': return 0xF;
    }
    throw string_t(string_t(traits::cs(L"Invalid character: ")) + c);
  }

  static void percent_decode(string_t& str)
  {
    typename string_t::iterator s2 = str.begin(), s3 = s2, s4 = s2;
    for (typename string_t::const_pointer s1 = str.data(), end = s1 + str.size(); s1 != end; ++s1, ++s2)
      if (traits::to_wchar(*s1) != L'%')
        *s2 = *s1;
      else {
        char_t d1 = *++s1;
        *s2 = char_t((hex_digit_value(d1) << 4) + hex_digit_value(*++s1));
      }
    // Decode UTF-8 string in-place
    while (s3 != s2) {
      unsigned u = *s3;
      if (u < 0x80)
        ;
      else if ((u >> 5) == 6) {
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u = ((u & 0x1F) << 6) + (*s3 & 0x3F);
      }
      else if ((u >> 4) == 0xE) {
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u = ((u & 0xF) << 12) + ((*s3 & 0x3F) << 6);
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u += *s3 & 0x3F;
      }
      else if ((u >> 3) == 0x1E) {
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u = ((u & 7) << 18) + ((*s3 & 0x3F) << 12);
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u += (*s3 & 0x3F) << 6;
        if (++s3 == s2 || (*s3 & 0xC0) != 0x80) continue;
        u += *s3 & 0x3F; 
      }
      else {
        ++s3;
        continue;
      }
      s4 = traits::from_utf32(u, s4);
      ++s3;
    }
    if (s4 != str.end()) str.resize(s4 - str.begin());
  }
};


// string helpers

template <class string_t, class stringview_t>
struct diff_match_patch_string_helpers
{
  static string_t to_string(int n)
  {
    string_t str;
    bool negative = false;
    std::size_t l = 0;
    if (n < 0) n = -n, ++l, negative = true;
    int n_ = n; do { ++l; } while ((n_ /= 10) > 0);
    str.resize(l);
    typename string_t::iterator s = str.end();
    constexpr static char digits[] = "0123456789";
    do { *--s = digits[n % 10]; } while ((n /= 10) > 0);
    if (negative) *--s = '-';
    return str;
  }
	/**
	 * @param str String to take a substring from.
	 * @param pos Position to start the substring from.
	 * @param len Length of substring.
	 * @return Substring.
	 */
  static inline stringview_t mid(const stringview_t &str,
                                   typename stringview_t::size_type pos,
                                   typename stringview_t::size_type len = stringview_t::npos)
  {
    return (pos >= str.length()) ? stringview_t() : str.substr(pos, len);
  }

  static inline stringview_t right(const stringview_t& str, typename stringview_t::size_type n)
  {
    return n > str.size() ? stringview_t() : str.substr(str.size() - n);
  }
};

// Unicode helpers
template <class char_t, class utf32_type = unsigned>
struct diff_match_patch_utf32_direct
{
  typedef utf32_type utf32_t;

  template <class iterator>
  static iterator to_utf32(iterator i, iterator /*end*/, utf32_t& u)
  {
    u = *i++;
    return i;
  }

  template <class iterator>
  static iterator from_utf32(utf32_t u, iterator o)
  {
    *o++ = static_cast<char_t>(u);
    return o;
  }
};

template <class char_t, class utf32_type = unsigned>
struct diff_match_patch_utf32_from_utf16
{
  typedef utf32_type utf32_t;

  static const unsigned UTF16_SURROGATE_MIN      = 0xD800u;
  static const unsigned UTF16_SURROGATE_MAX      = 0xDFFFu;
  static const unsigned UTF16_HIGH_SURROGATE_MAX = 0xDBFFu;
  static const unsigned UTF16_LOW_SURROGATE_MIN  = 0xDC00u;
  static const unsigned UTF16_SURROGATE_OFFSET   = (UTF16_SURROGATE_MIN << 10) + UTF16_HIGH_SURROGATE_MAX - 0xFFFFu;

  template <class iterator>
  static iterator to_utf32(iterator i, iterator end, utf32_t& u)
  {
    u = *i++;
    if (UTF16_SURROGATE_MIN <= u && u <= UTF16_HIGH_SURROGATE_MAX && i != end)
      u = (u << 10) + *i++ - UTF16_SURROGATE_OFFSET; // Assume it is a UTF-16 surrogate pair
    return i;
  }

  template <class iterator>
  static iterator from_utf32(utf32_t u, iterator o)
  {
    if (u > 0xFFFF) { // Encode code points that do not fit in char_t as UTF-16 surrogate pairs
      *o++ = static_cast<char_t>((u >> 10) + UTF16_SURROGATE_MIN - (0x10000 >> 10));
      *o++ = static_cast<char_t>((u & 0x3FF) + UTF16_LOW_SURROGATE_MIN);
    }
    else
      *o++ = static_cast<char_t>(u);
    return o;
  }
};

// Specialization of the traits for wchar_t
#include <cwctype>
template <class string_t, class stringview_t>
struct diff_match_patch_traits<wchar_t, string_t, stringview_t>
: diff_match_patch_utf32_from_utf16<wchar_t>
, diff_match_patch_string_helpers<string_t, stringview_t>
{
  static bool           is_alnum  (wchar_t c)        { return std::iswalnum(c)? true : false; }
  static bool           is_digit  (wchar_t c)        { return std::iswdigit(c)? true : false; }
  static bool           is_space  (wchar_t c)        { return std::iswspace(c)? true : false; }
  static bool           is_control(wchar_t c)        { return (c == L'\n' || c == L'\r');     }
  static uint32_t       to_uint32 (const string_t& str) { return static_cast<uint32_t>(std::wcstoul(str.c_str(), nullptr, 10)); }
  static wchar_t        from_wchar(wchar_t c)        { return c; }
  static wchar_t        to_wchar  (wchar_t c)        { return c; }
  static const wchar_t* cs        (const wchar_t* s) { return s; }

  static const wchar_t eol = L'\n';
  static const wchar_t tab = L'\t';
  static const wchar_t pilcrow = L'\u00b6';
};

// Possible specialization of the traits for char
#include <cctype>
template <class string_t, class stringview_t>
struct diff_match_patch_traits<char, string_t, stringview_t>
: diff_match_patch_utf32_direct<char>
, diff_match_patch_string_helpers<string_t, stringview_t>
{
  static bool     is_alnum  (char c)              { return std::isalnum(c)? true : false;   }
  static bool     is_digit  (char c)              { return std::isdigit(c)? true : false;   }
  static bool     is_space  (char c)              { return std::isspace(c)? true : false;   }
  static bool     is_control(char c)              { return (c == '\n' || c == '\r');        }
  static uint32_t to_uint32 (const string_t& str) { return static_cast<uint32_t>(std::strtoul(str.c_str(), nullptr, 10)); }
  static char     from_wchar(wchar_t c)           { return static_cast<char>(c);            }
  static wchar_t  to_wchar  (char c)              { return static_cast<wchar_t>(c);         }
  static string_t cs        (const wchar_t* s)    { return string_t(s, s + std::wcslen(s)); }

  static const char eol = '\n';
  static const char tab = '\t';
  constexpr static const char* pilcrow = "\xC2\xB6";
};


#endif // DIFF_MATCH_PATCH_H
