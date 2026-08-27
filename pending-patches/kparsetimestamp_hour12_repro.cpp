// kParseTimestamp: hour 12 with an am/pm meridiem is wrongly rejected.
// Self-contained repro. Build with dekaf2 (see TODO for the one-liner).
#include <dekaf2/ktime.h>
#include <dekaf2/kstring.h>
#include <cstdio>
using namespace dekaf2;

static const char* R(KStringView fmt, KStringView val)
{
    static KString s;
    auto t = kParseTimestamp(fmt, val);
    if (!t.ok()) return "**FAIL (Invalid)**";
    s = kFormTimestamp(KUTCTime(t), "{:%Y-%m-%d %H:%M:%S}");
    return s.c_str();
}

int main()
{
    // dekaf2's OWN documented 12-hour format, verbatim from the format table in
    // ktime.cpp (~line 1056):  "NNN DD, YYYY hh:mm:ss aa"  e.g. "Dec 02, 2017 2:39:58 AM"
    const char* F = "NNN DD, YYYY hh:mm:ss aa";

    printf("Same format string for every row: \"%s\"\n\n", F);
    printf("  %-28s  %-9s  %s\n", "input", "24h-clock", "kParseTimestamp result");
    printf("  %-28s  %-9s  %s\n", "-----", "---------", "----------------------");
    for (int h = 1; h <= 12; ++h) {
        char am[64], pm[64];
        snprintf(am, sizeof am, "Jun 15, 2024 %02d:00:00 AM", h);
        snprintf(pm, sizeof pm, "Jun 15, 2024 %02d:00:00 PM", h);
        int am24 = (h == 12) ? 0 : h;
        int pm24 = (h == 12) ? 12 : h + 12;
        printf("  %-28s  %02d:00:00   %s\n", am, am24, R(F, am));
        printf("  %-28s  %02d:00:00   %s\n", pm, pm24, R(F, pm));
    }

    printf("\nControl group (proves the harness itself is fine):\n");
    printf("  ISO auto-detect     '2024-06-15 12:00:00' -> %s\n",
           R("YYYY-MM-DD hh:mm:ss", "2024-06-15 12:00:00"));
    printf("  11 AM (hour 1..11)  same format           -> %s\n",
           R(F, "Jun 15, 2024 11:00:00 AM"));
    printf("  12 AM (the defect)  same format           -> %s\n",
           R(F, "Jun 15, 2024 12:00:00 AM"));
    return 0;
}
