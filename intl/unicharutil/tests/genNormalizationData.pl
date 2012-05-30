#!/usr/bin/perl 
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

open ( TEXTFILE , "< NormalizationTest.txt")
    || die "Cannot find NormalizationTest.txt. The latest version should be available from\n http://www.unicode.org/Public/UNIDATA/NormalizationTest.txt\n";

open ( OUT , "> NormalizationData.h")
#open ( OUT , "> test.txt")
    || die "Cannot create output file NormalizationData.h\n";

$mpl = <<END_OF_MPL;
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* 
    DO NOT EDIT THIS DOCUMENT !!! THIS DOCUMENT IS GENERATED BY
    mozilla/intl/unicharutil/tools/genNormalizationData.pl
 */
END_OF_MPL

print OUT $mpl;

# XXX This code assumes that wchar_t is 16-bit unsigned, which is currently
#      true on Windows, Linux and Mac (with |g++ -fshort-wchar|).
#      To make it work where that assumption doesn't hold, one could generate
#      one huge array containing all the strings as 16-bit units (including
#      the 0 terminator) and initialize the array of testcaseLine with pointers
#      into the huge array.

while(<TEXTFILE>) {
    chop;
    if (/^# NormalizationTest-(.+)\.txt/) {
	print OUT "static char versionText[] = \"$1\";\n";
    } elsif (/^\@Part(.)/) {
	if ($1 != "0") {
	    print OUT "  {\n";
	    print OUT "    L\"\",\n";
	    print OUT "    L\"\",\n";
	    print OUT "    L\"\",\n";
	    print OUT "    L\"\",\n";
	    print OUT "    L\"\",\n";
	    print OUT "    \"\",\n";
	    print OUT "  },\n";
	    print OUT "};\n";
	}
	print OUT "\n";
	print OUT "static testcaseLine Part$1TestData[] = \n";
	print OUT "{\n";
    } else {
	unless (/^\#/) {
	    @cases = split(/;/ , $_);
	    print OUT "  {\n";
	    for ($case = 0; $case < 5; ++$case) {
		$c = $cases[$case];
		print OUT "    L\"";
		@codepoints = split(/ / , $c);
		foreach (@codepoints) {
		    $cp = hex($_);
		    if ($cp < 0x10000) {
                      # BMP codepoint
			printf OUT "\\x%04X", $cp;
		    } else {
                      # non-BMP codepoint, convert to surrogate pair
			printf OUT "\\x%04X\\x%04X",
			           ($cp >> 10) + 0xD7C0,
			           ($cp & 0x03FF) | 0xDC00;
		    }
		}
		print OUT "\",\n";
	    }
	    $description = $cases[10];
	    $description =~ s/^ \) //;
	    print OUT "    \"$description\"\n";
	    print OUT "  },\n";
	}
    }
}
 
print OUT "  {\n";
print OUT "    L\"\",\n";
print OUT "    L\"\",\n";
print OUT "    L\"\",\n";
print OUT "    L\"\",\n";
print OUT "    L\"\",\n";
print OUT "    \"\",\n";
print OUT "  },\n";
print OUT "};\n";
close (OUT);
close (TEXTFILE);
