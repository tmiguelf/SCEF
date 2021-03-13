Build status: [![Build status](https://ci.appveyor.com/api/projects/status/5steahi2rot36drj/branch/master?svg=true)](https://ci.appveyor.com/project/tmiguelf/scef/branch/master)

# SCEF
	Simple Configuration Exchange Format
It’s a format that aims to provide a simple way of representing structured configuration, by removing some of the pains regarding reading/writing/interpreting configuration files, where the main focus is to make it easy for humans to use.

Formats like XML can be unnecessarily verbose (insisting that you close tags with exactly the same name as they were open), with weird escaping sequences that makes it hard to read, control characters that are easy to mix in regular text but which would invalidate the entire document (even if the intention was unambiguous), ambiguous space handling rules (making it hard to write). Other formats like JSON can be very well defined, but are also too strict. Who hasn’t added that extra unnecessary comma at end of an array making the parser fail without warning as to why or where the problem occurred? (and more importantly how to fix it).

SCEF is aimed for simplicity, simple to follow rules that would translate to easier to understand documents which hopefully shall translate to less mistakes in composing and interpreting a document. 
Example document
```
!SCEF:v=1

<Network:
	IP = 192.168.1.3;
	port = 2525;
>
<translations:
	<"IP editbox description":
		"IP address of coordinator server.^n"
		"IPv4, IPv6, or DNS are acceptable inputs.^n"
		"If a DNS is specified, please make sure that the "
		"DNS server is correctly configured" 
	>
>

```

## Goals 
There are 2 distinct types of goals that are important to distinguish
 * The format goals
 * The library goals

The format goals are aimed at design features of the document itself regardless of the parser being used, the library goals are aimed at the design features of the parser specifically written in code in this repository.

### Format goals
There’s no official organization curating the specifications of the format, and as of yet, the format is whatever rules I deemed fit to suit my own use cases. But still, there are rules and guidelines targeted to specific use cases, rules that have been arrived at trough experience in using other formats in different types of usage scenarios.
Among them are:
* It should be easy for computers to parse
* It should be easier for humans to read and write. Easier for humans rather than just computers is the most important aspect.
* Interpretation should be as unambiguous and deterministic as possible without being excessively strict. There should only be one way to interpret what was written.
* Format should be able to handle Unicode (but not necessarily be strictly compatible). Such that it can be used for example as a translation file and support programs that require internationalization. This also means supporting different encodings (ANSI, UTF8, UTF16, UCS2, UCS4/UTF32)
* It should be possible to use existing text editors to edit a file formatted in SCEF (without the use case being limited to files). Avoiding the necessity of extra tooling.
* It should be possible to identify the version of the format that it was written on. Such that when the format inevitably evolves, it would be possible to write parsers capable of reading/writing multiple versions of the format. Thus, easing the migration process of applications trying to move to a more recent standard while still providing backwards compatibility.
* It should be possible to represent simple structured data. Like lists, elements with child elements up to an arbitrary depth, associations, or just a simple value.
* It should be possible to write annotations without it being confused with content (comments)

### Library goals
* It should be able to support custom streams for reading a writing. This will allow to work with SCEF documents, not only from the file system, but with documents from other sources. For example, to make it possible in networked application to be able to save a SCEF document to memory, send over network, receive and interpret the document from memory without ever touching the file system. It could also be used for example to create a stream that reads from a compressed file directly without having to first extract the contents to disk. The parsing of the document should be independent of the medium. It’s up to the user of the library to decide the medium of the document.
* It should be able to support multiple encodings, at least ANSI, UTF8 and UCS4 (big-endian and a little endian), any other extra encodings are welcome (please see full list of currently supported encodings sections. It should be able to automatically detect the encoding on loading and handle it correctly. It should also allow the user of the library to select an encoding that they wish to use on saving (as long as it is implemented). This also means informing the user what encoding was used when loading a document such that they can opt to save the document with the same encoding.
* It should be able to expand in case of future format versions.
* In case the document contains formatting mistakes/errors, the library shall provide means to inform exactly what the problem is and where in the document the problem occurred, as well as any additional context that may be relevant for an application to provide better error handling.
* It should provide the means for library users to control how strictly the formatting rules should be followed in situations where rules have been violated (but where the parser has enough context to infer intent and continue parsing).
* It should be quick and efficient.
* Within reasonable limits, the library should provide the means to the user to preserve indentation and other visual nuances (empty lines, comments, etc..), in load, modify, save workflows. I.e. It should allow for the application to load an existing file, edit only a small portion, and leave untouched section of the document untouched. (Please see FAQ)
* It should also be possible for library users to discard any irrelevant information (such as spacing or comments, in direct contrast to the previous point) in case these are irrelevant for an application, and the user does not want to pay for the extra overhead.
* Should be user friendly and safe. I.e. providing sensible and predictable means to be able to browse and edit elements of the document easily, assist in the interpretation of the content, and be able cleanup after itself without leaking resources.

## Format specifications
A SCEF document is broken down into 2 major parts:
 * Header
 * Body

### Control characters
 * `!` - Only a control character in the header of the document, and it commits the line to having the header information. On the body of the document the exclamation mark is just a regular character.
* `<` - Opens a group
* `>` - Closes a group
* `=` - Indicates an assignment of a value to a key. Only when an `=` sign is present does a value become a key (or a key-value pair).
* `:` - Indicates the ending of a group header and the start of a group body
* `,` - Terminates a "singlet" or a "key-value" pair, separating them from subsequent items
* `;` - Terminates a "singlet" or a "key-value" pair, separating them from subsequent items
* `'` - Starts/Ends a single-quote escape block.
* `"` - Starts/Ends a double-quote escape block
* `^` - Within an escape block, it starts a code point escape sequence. Outside of a escape block it’s treated as a regular character.
* `#` - Starts an inline comment.
* Line Feed (0x0A)- Interrupts and closes any sequence, except for a group body. If within a single/double-quote escape block, it closes that escape block (it’s not possible for the escape block to cross to a new line). If within an inline comment, it closes the comment. If a key expects to capture a value, it stops trying to get any extra data. If a group header is expecting a group name, it closes the group header and starts the group body.
* Any space except for line feed – Terminates a string sequence as long as it is outside of an escape block. For example, if the following sequence appears `value1 value2`, the space will separate `value1` from `value2`, making it 2 distinct entities.  Only the tab (0x09) and the space (0x20) are canonically considered as a space. However, the vertical tab (0x0B), form feed (0x0C), and carriage return (0x0D) may also be included into this definition but only as an extension.
* Non valid character - Any non-space codepoint bellow 0x20 shall cause an instant parsing fail.

### Header
The first bytes of the document should contain [Byte Order Mark](https://en.wikipedia.org/wiki/Byte_order_mark), which shall be used to determine the encoding of the rest of the document. if no recognizable BOM is found the document is assumed to be ANSI encoded.
The first none-space characters should be as follows:
```!SCEF:v=0```
Where `0` is placeholder for a number between 0 and 65535.
And the contents are interpreted as follows:
 * `!` - The exclamation mark indicates that the header is present in this line. If any other non-space character is present before `!` it is considered that the document has no header, and the parsing may fail. All subsequent elements of the header must be present in this line, or else it’s not a valid SCEF document and the parsing will fail.
* `SCEF` - Is the signature of the document, indicating that this is a SCEF document.
* `:` - Separates the document’s signature from de document’s version.
* `v` - Inidcates that a version information follows
* `=` - Indicates that the value of the version will follow
* `0` - Is the version of the format, it can be a number between 0 and 65535
* New line - Indicating the end of the header
There can be any space character between any of the elements except for new line.
The signature, as well as the version indication can be in either upper or lower case.\
Examples of valid header:
 * `!SCEF:v=0`
 * `   ! scef : V = 123 `
 * `     !      ScEf     :              v           =    23    `

Examples of non-valid headers:
 * `scef:v=0` (`!` is missing)
 * `!SC EF:v=0` (there’s a space in the signature, the SCEF block must be a full block)
 * `!SCEF ` (version is missing)
 * `!:v=` (signature is missing)
 * `!SCEF:v=`, `!SCEF:v=1 2`,  `!SCEF:v=A`, `!SCEF:v=123456` (version is invalid or out of range)
 
(list is not exhaustive)

There should be no other elements, including comments, either before the header or in line with the header.
### Body
The body is where the content of the document begins proper, this is where data structures (or items) are encoded.
There are 3 types of data containing items (structures), and 2 types of non-data containing items.
In the data containing items we have:
 * Singlet - A single individual string.
 * Key-value - This structure is a string to which another string is associated.
 * Group - A structure that can contain child items.
 
In the non-data containing items we have:
* Spaces - To make the text more presentable and readable to humans.
* Comments – For annotations in the document

#### Singlet
A singlet is a data structure that represents a single individual value on its own.
Any non-control non-space character sequence, or opening of an escape block, can define a singlet as long as it does not follow the opening of a group item (i.e. `<`), nor it follows or is followed by an assignment (i.e. `=`).
Singlets should be followed by a separation control character (';' or ','), however parsers are free to decide to accept the singlet if the separator is missing.

Ex. Of well-defined singlets:
 * `SingleWord;`
 * `"Many words in an escape block";`
 * `Singlet` - Ok, separator control character is missing, but input can still be accepted by a lenient parser.
 * `<: Singlet;` - Ok, group header is closed before singlet is defined
 * `=; Singlet;` - Ok, previous assignment is closed before definition
 * `Singlet; =` - Ok, singlet definition is closed before assignment
 * `<groupName Singlet;`  - Ok, Singlet follows the opening of a group and the definition of the group name, the group header is not closed, but the presence of a space makes the sequence "Singlet" a separate entity from the header.
 * 
``` 
<
 Singlet1	#Ok, group header was not closed, but sequence is in a different line 
= 		# Still Ok, Singlet1 is followed by an = sign, but this is in a different line
 Singlet2;	#Ok, Singlet2 follows a = sign but is in a different line
```

Ex. Of poorly defined singlets:
* `<     "is this a Singlet?"` - Not ok, sequence follows inline the opening of a group. This would make it the name of the group and not a singlet.
* `singlet = ` - Not ok, sequence is followed by an assignment, which would make it a key of a key-value pair
* `= singlet` - Not ok, sequence follows an assignment, which would make it value in a key-value pair
* `is this a single singlet? ` - Not ok, there’s no escape block, and there are spaces between the elements, making it 5 different singlets with the content `is`, `this`, `a`, `single`, and `singlet?`

#### Key-Value
A key-value is a data structure that associates one string to another, and this is expressed by means of the `=` control character, as long as the first string could not be confused by a group name. The first string is referred as the "Key", while the second one is referred as the "Value". Key-value pair associations must be defined in line and in sequence.
Key-value pairs should be followed by a separation control character (';' or ','), however parsers are free to decide to accept a key-value pair if the separator is missing.

Ex. Of well-defined key-value pairs:
* `key=value;`
* `"Multi Word key" = "Multi word value";`
*  `key=value` - Ok, separator control character is missing, but input can still be accepted by a lenient parser.
* `<: key=value;` - Ok, group header is closed before the definition of the key
* `=value;` - Ok, empty key with non-empty value.
* `key=;` - Ok, key with empty value.
* `=;` - Ok, empty key with empty value.
* `<groupName key = value;`  - Ok, key follows the opening of a group and the definition of the group name, the group header is not closed, but the presence of a space makes the sequence "key" a separate entity from the header.
``` 
<
 key = value # ok, group header not closed, but key occurs on a new line
```

Ex. Of poorly defined key-value pairs:
 * `<  key=value` - Not ok, sequence follows inline with a group opening. This is interpreted as a group named "key" with a key-value pair with an empty key.
 * `mutli word key = multi word value` - Not ok, the presence of spaces outside an escape block breaks the sequence, being interpret as singlet `multi` followed by singlet `word` followed by a key-value pair `key = multi` followed by singlet `word` followed by singlet `value`.
*
```
key = 		#Not ok – key with empty value followed by 
value; 		# singlet

key		#Not ok, singlet followed by
= value; 	# key-value with empty key
```
#### Group
A group is a structure that can contain child items of any other type.
The opening of a group is indicated by the control character `<`, inline with the opening control character a non-space string sequence will define the name of the group (this can also be referred to as the header of the group). In line with the opening of a group, the control character `:` is expected to indicate the end of the group header, however parsers are free to accept a missing `:` in case the closing of the group header can be deduced from the context.
The closing of a group can be indicated by the control character `>`.
Example of well-defined groups:
 * `<name:>`
 * `<"group name":>`
 * `<:>` - Ok, group defined with an empty name
 * `<name>` - Ok, lenient parser may accept the missing header closing control character (`:`)
 * `<>` - Ok, group with empty name, and lenient parser may accept the missing header closing tag (`:`)
 * `<group: singlet>` - Ok a group named `group` containing a singlet named `singlet` 
 * 
```
< group: #open group named "group"
	singlet;  #child singlet
	key = value; #child key-value pair
> #closing of the group
```
Example of poorly defined groups:
 * `< group name:>` - Not ok, space in non-escape block interrupts the group header. This creates a group named `group` containing the singlet `name`.
 * `<group:` - Not ok, missing closing control character `>`. 
 *
```
<
group_name #not ok, name is defined in a separate line of the opening control character
# this will be interpreted as a group with an empty name with a child singlet named "group_name"
>
```
#### Spacer
Any space character sequence outside of an escape block is a space

#### Comment
An inline comment can be started with the control character `#` as long as it occurs outside of an escape block. After an inline comment is open, any character sequence including any control character except for the line feed, are part of the comment. Only a line feed may close an inline comment.
Ex. `#This is a comment`
There are no multi-line comments

### Escaping
Escaping can only occur inside an escape block. An escape block can be opened by either a single-quote (`'`) or a double-quote (`"`) control character and must be closed by the same controlled character as it was opened.\
If a newline appears before the escape block is closed, the escape block shall be closed regardless, the parser may accept a document with an unclosed escape block, but an escape block may not capture content past the end of the line.\
An escape block open with a single-quote can contain unescaped double-quote characters, and an escape block open with a double-quote can contain unescaped single-quote characters.\
All other control characters, except for line feed, invalid codepoints, and the character `^`, may appear unescaped within an escape block.\
An escape sequence may be started within an escape block with the control character `^`.\
The following escape sequences are accepted:
 * `^^` - Escapes a single character `^`
 * `^'` - Escapes `'`
 * `^"` - Escapes `"`
 * `^n` - Escapes a line feed (0x0A)
 * `^t` - Escapes a tab (0x09)
 * `^r` - Escapes a carriage return  (0x0D)
 * `^00` - Where 00 are exactly 2 hexadecimal digits between 00 and FF, escaping the respective code point.
 * `^u0000` - Where 0000 are exactly 4 hexadecimal digits between 0000 and FFFF, escaping the respective code point.
* `^U00000000` - Where 00000000 are exactly 8 hexadecimal digits between 00000000 and FFFFFFFF, escaping the respective code point.

## Supported encodings
Currently the library supports the following encodings:
 * ANSI
 * UTF8
 * UTF16 little endian
 * UTF16 big endian
 * UTF32/UCS4 little endian
 * UTF32/UCS4 big endian

## Line endings
Given that the carriage return is currently considered as a space in this library, and that trailing spaces are pruned at the end of the line on save, this means that only LF line endings are officially supported (since the CR in a CRLF would be automatically pruned on save).
However, documents with CRLF line endings will still be read without an issue.
I may consider supporting CRLF line endings on save in the future, but the at the moment is just not worth the effort.

## Library implementation
Please see README.md in the project directory

## Integration with notepad++
You can integrate SCEF with notepad++ by importing the custom markup file located in the "extra" directory.

## FAQ
**Q: Why when I load a document and save it back into the file in a "save as loaded" environment, and all the trailing spaces before the end of a line are removed?**\
A: Spaces are not visible, and they don’t provide any formatting benefit when they occur at the end of a line. They are simply not worth the effort to handle and store them.\
I personally hate trailing spaces at the end of a line, and whenever I see them, I tend to just remove them, and if this can happen as a feature so much the better. I’m not convinced that it is worth the extra effort to add extra code just to put those trailing spaces back.
And yes, I do realize that this means that files original loaded with `CrLf` as their line ending will now be saved as `Lf`, I may be convinced to put in an option to allow the choice of line ending on save. But any modern text editor is capable of handling `Lf` line endings just fine on almost any platform, they are just not worth the effort for the problems they cause.

**Q: I’m trying to work on a "save as loaded" environment, but the document on save converts characters that were escaped in the original document into their unescaped form (or change the escaping format). How can I preserve the original escape sequence when saving?**\
A: Unfortunately, You can’t. I really did whish that these could be easily integrated as a feature. This parser is not a real "save as loaded" type of parser, it’s just a "does its best within reasonable". The challenge comes from having to keep track of 2 different texts, one for how the document should be interpreted for an application reading the document, and the other one for how it was actually written in the document. It would make things either too complicated to write code for, or pay excessive overhead to do the translation of the escaped sequence on read. And things only get more complicated when an application tries to modify data. The effort to keep track of that information was simply not worth it.

**Q: I’m trying to work on a "save as loaded" environment, but the document on save now adds extra control characters (like the `;` and the end of a value) that were missing in the original document. Can I chose not add these back?**\
A: Unfortunately, No. For similar reasons as before. It’s just to much effort to keep track of the extra information regarding either or not those controls characters were present in the original document. And don’t get me wrong, those control characters aren’t optional in the format, it’s just that the parser can deduce your intention and lets you load the document anyways. It is still doing you a favor by fixing the document.

**Q: I want to represent a list type structure in SCEF, but there’s no equivalent of that in SCEF. Why aren’t lists included?**\
A: Well actually they are if you think about it. Consider first what a list is, and how would you actually implement it in SCEF.
Let’s say that you would define a list by starting with the name of a list, an assignment then the group of elements in the list like so:
```
List_Name = [Value1, Value2, Value3, etc..];
```
Essentially what a list is-is just a structure with direct child elements. Now what are the properties of this child elements? Can they only contain single individual values? Or could they be other structures like key-value pairs? Or even a list of lists?\
If it were me, I would want to be able to represent any arbitrary data onto a list.\
But keep thinking about it, that `[` mark after the equals sign, may make it a bit troublesome to parse since while reading the algorithm has to delay the decision of either to  treat it as a key-value pair or a list. So let’s help the parser along by defining it in advance that we are going to define a list, so we move the `[` to the beginning before the list name like so:
```
[List_Name = Value1, Value2, Value3, etc..];
```
In terms of data expressiveness nothing has really changed, it’s still a list.\
But notice now as I pull off this magic. I replace the control characters `[` for a `<`, `=` into `:`, and `];` to `>`. We get:
 ```
<List_Name : Value1, Value2, Value3, etc..>;
```
In terms of expressiveness, nothing has really changed, but now it’s identical to the "group" concept, and that already exists. 
The "group" is a list, it is also an array, it’s a thing that has child things. Adding more abstractions to do the same thing seemed redundant.

**Q: I need to encode data in multiple lines. How can I make SCEF continue to parse an element into the next line?**\
A: The short answer is, You can’t! I may consider on a future version of the specification to introduce this feature, but for the moment I simply don’t see the need for it.\
Let me ask you a different question instead, should you be able to encode data into multiple lines?
The intuitive answer is definitely "Yes"! How could you for example support the usage of this format for applications like translation files, that can easily have text that spans multiple lines?
I can tell you what I do in those situations. Which is to use a structure like this:
```
<Token:
"Line 1 of the text^n",
"Line 2 of the text^n ",
"Etc.."
>
```
I.e. create a group that can contain child-items. Then the interpretation of this structure when used in the translation context would be to combine the different singlets in the group and glue them together into a single string.\
But let’s say that my application instead is to statically define a list of Ips.
Should this be a valid configuration?
```
	IP = "127.0
	.0.1"
```
Probably not.\
The application, not the parser, would solve the problem by interpreting the data in a different way (and combining data present in multiple lines), let the application decide if it's in a context where multi-line text is acceptable or not.

**Q: Why are there no multiline comments**\
A: The short answer is, you don’t need to. A more accurate answer would be, it’s complicated.\
Let’s say for example that `/*` would open a multi-line comment (like in C) and that `*/` would close it.\
And that you have the following structure:
```
<Network:
	addr=google.com
	port=26; 
/* The alternative port is used in case the primary port is inaccessible
3 connections are attempted on the primary port
before the switch to alternate port takes place */
	alternate_port=42;
>
```
But since I’m testing the application, I don’t want it to be able to connect to the Network and I decide that I’m going to do this by commenting out the "network" group. Like this:
```
/*<Network:
	addr=google.com
	port=26; 
/* The alternative port is used in case the primary port is inaccessible
3 connections are attempted on the primary port
before the switch to alternate port takes place */ #HO NO!
	alternate_port=42;
>*/
```
If you are here, you probably know a little bit of C++, and have experience with its multi-line comments. And you would have easily noticed the problem that the comment block was prematurely closed on line 6 instead of the expected line 8.\
You will easily find that it is very difficult to properly handle multi-line comments in a document that already has annotations.
What a parser would need to do in order to resolve these cases is to keep interpreting the document and keeping track of how many open and close comment blocks exists within a multi-line comment block, even if it had absolutly no use for it.\
But you can achieve the same result as a multi-line comment by doing the following:
```
<???Network:
	addr=google.com
	port=26; 
# The alternative port is used in case the primary port is inaccessible
# 3 connections are attempted on the primary port
# before the switch to alternate port takes place
	alternate_port=42;
>
```
If your interpreter was written in a way as to ignore content that it doesn’t recognize, the fact that you changed the name of the group would be sufficient to hide the intended block.\
Again, the application, not the parser, can do this job for you.\
Or alternative just get an editor that is capable of adding inline comments automatically to the selected text, like notepad++ or something :)

**Q: Why are codepoints bellow 0x32 (except for spaces) immediately causes the parser to stop reading the document? Why couldn’t it issue a warning and let the user decide if this should be incorporated into regular names?**\
A: Actually, this behavior was part of the original implementation of the parser, in fact there’s nothing special or different regarding this code points from a code handling perspective.\
Unfortunately, specials rules had to be intentionally put in to invalidate and interrupt the parsing of the document. The original implementation was so permissive that could lead to the ridiculous situation of being able to read any random sequence of bytes as valid SCEF document. This is not ideal, because of many reasons (including faulty user input) circumstances may actually lead you to try and load an invalid file as a SCEF document regardless of either or not it should. And if we can detect that something is wrong earlier on, we can avoid the need to get to the interpretation stage before realizing that the data doesn’t make any sense because you were actually trying to load a dll or something as a SCEF document.\
Code points in this range are not printable characters, and they are not on your keyboard. Although it is definitely possible type them in a text editor, you need to put some intentional effort into it just to achieve some dubious application. On the other hand, random binary files are very likely to contain this code points just out of the natural bias for low numbers in programming. If you find this code points in a document, chances are that this is how they got in.\
But in the rare case that you have a legitimate use for this codepoints (and I personally had application where this was the case), do not worry, SCEF has you covered, all you have to do is to escape these codepoint. They won’t show up in the document, but they will show up in your application.

## TODO:
 * There’s a known bug where attempting to save codepoints over 0xFF in an ANSI formatted document will fail, however these code points should instead be escaped in accordance to the escape rules, this will be fixed on future work by adding a mechanism where the encoder reports the range of code points that it can effectively support unescaped.
 * Current library implementation is not as efficient as it could be. This library is still in an early development stage, and serves more as a proof of concept. Future work will require to reflow some of the algorithms used to load and save in order to increase performance.
