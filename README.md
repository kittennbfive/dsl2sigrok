# dsl2sigrok

## What does this do?
This is a small tool that converts the native file format of DSView to a file that Sigrok/Pulseview can load. It is written in C, for Linux only and released under AGPLv3+ WITHOUT ANY WARRANTY. Tested on Debian 11.

## DSView?
Thats a Sigrok/Pulseview-fork made by DreamSourceLab for their Logic Analyzers. It works but i only use it for capturing data (because Pulseview still had some trouble with my LA last time i checked, especially with complex triggers and stuff like this - maybe this has improved?) and then use Pulseview to analyze/process the recorded data.

## Why did i made this?
DSView has an export-function, but it is really, *really* slow. Saving in the native format (.dsl) only takes seconds, exporting to .sr (or .srzip actually, thats not correct) takes minutes or even hours(!). This is not acceptable for me so i looked at the file format and made this tool.

## Some numbers
### Demo-device, 16 channels, 5s, 20MHz
DSView save as .dsl: a few seconds  
DSView export as .srzip: 16 minutes(!), 2,5MB file size, 12210 small data files inside the .sr (.sr is basically .zip if you didn't know, as is .dsl)  
dsl2sigrok: 8 seconds, about 950kB file size, 50 data files inside the .sr

### Dummydata, 12 channels, 5s, 100MHz
This data was created by some Perlcode and is random data, about 700MB.  
DSView will open the file without any problems, but exporting would take about 4 hours(!) (calculated, i stopped at 2% after 5 minutes)  
dsl2sigrok: 1,5 minutes

### Dummydata2, 12 channels, 30s, 100MHz
This was created by some Perlcode too and contains about 4GB worth of captured data. I don't think you can even capture so much with DSView (or maybe you can depending on your hardware, i don't know)  .
DSView will open the file but only display a small part of the data. I did not even try exporting it...  
dsl2sigrok: 8 minutes

## Sounds good. Any limitations/drawbacks?
Yes, of course. The main problem of this tool is RAM usage, it can go up to several GB while the tool runs for really big captures. This is mainly due to the way libzip works (see comment in code). I could have implemented the use of temporary files but that would make things slower and the main goal of this tool is speed.

### Details on RAM-usage
Dummydata (700MB of captured data): ~1GB RAM needed  
Dummydata2 (4GB of captured data): ~5.6GB RAM needed  
However keep in mind that this tool normally will only run for seconds or minutes, so on a decent machine with enough RAM that shouldn't be a too big problem.
  
Also note that this tool is single-thread which obviously limits the speed. Using several parallel threads could improve speed dramatically but would make the thing much more complicated too...

## External dependencies?
Yes, [libzip](https://libzip.org/). On Debian you need `libzip-dev`. I wrote a little wrapper around this for convenience.

## How to compile?
`gcc -Wall -Wextra -Werror -O3 -o dsl2sigrok main.c zip_helper.c -lm -lzip`

## How to use?
>usage: dsl2sigrok $input_file $output_file \[$compression_ratio\]  
>$compression_ratio is optional and must be between 1 and 9 (fastest to best compression)  

Using a higher compression ratio *can* reduce output file size depending on your data, that might be useful for archiving. However a higher compression ratio will take more time to process of course. In doubt just try.

## Some notes about the code
* I use the word "block" when referring to the input-data (.dsl) and "chunk" when referring to the output data (.sr).
* By default this tool will create chunks of 4MB, this seems to be the default for Pulseview. However you can increase this value quite a bit, Pulseview won't complain and the tool might run a bit faster (and you might get better compression / a smaller output file - i didn't really look into this).
* By default this tool supports up to 32 probes/channels, if you need more change the #define (untested).

