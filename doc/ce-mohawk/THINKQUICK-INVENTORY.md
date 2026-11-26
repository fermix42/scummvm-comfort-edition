# ThinkQuick step-1 inventory tables

Generated metadata; no original interpreter execution or game-support claim.

Counts cover the extracted installation including Custom Questions. Every declared
BCOD byte is decoded using the pinned original token reader; expression roots align.
Operator bytes are tokens here, so these counts differ from the old prefix scanner.

## Opcodes

| Token | Count | Inline operand | Original evaluator handler |
| --- | ---: | --- | --- |
| 0x00 | 668 | none | 0x405109 |
| 0x01 | 39305 | word | 0x403b5a |
| 0x04 | 34729 | none | 0x403b88 |
| 0x05 | 29343 | literal | 0x403bfe |
| 0x06 | 54661 | word | 0x403c1b |
| 0x07 | 58675 | none | 0x405109 |
| 0x08 | 87795 | none | 0x405109 |
| 0x0b | 8983 | none | 0x405109 |
| 0x0e | 81 | none | 0x405109 |
| 0x10 | 1604 | none | 0x405109 |
| 0x11 | 226 | none | 0x405109 |
| 0x12 | 93 | none | 0x405109 |
| 0x13 | 92 | none | 0x405109 |
| 0x14 | 250 | none | 0x405109 |
| 0x15 | 369 | none | 0x405109 |
| 0x16 | 1256 | none | 0x405109 |
| 0x17 | 106 | none | 0x405109 |
| 0x18 | 2 | none | 0x405109 |
| 0x19 | 2 | none | 0x405109 |
| 0x1a | 383 | none | 0x403cb8 |
| 0x1b | 3103 | none | 0x403d71 |
| 0x1d | 280 | none | 0x405109 |
| 0x1e | 415 | none | 0x405109 |
| 0x1f | 72 | none | 0x405109 |
| 0x20 | 27130 | none | 0x403d71 |
| 0x21 | 111 | none | 0x405109 |
| 0x22 | 18648 | none | 0x405109 |
| 0x23 | 1405 | none | 0x405109 |
| 0x24 | 1112 | none | 0x405109 |
| 0x25 | 3262 | none | 0x405109 |
| 0x2a | 2583 | none | 0x405109 |
| 0x31 | 6108 | word | 0x403bfe |
| 0x32 | 23 | none | 0x405109 |
| 0x34 | 12 | none | 0x405109 |
| 0x35 | 1627 | none | 0x405109 |
| 0x37 | 134 | none | 0x405109 |
| 0x4d | 9841 | command_word | 0x403c68 |
| 0x4e | 4286 | command_word | 0x403c98 |
| 0x4f | 5 | command_word | 0x403ca8 |
| 0x61 | 3126 | none | 0x404eca |
| 0x62 | 3551 | none | 0x404dda |
| 0x63 | 7181 | none | 0x404d70 |
| 0x66 | 103 | none | 0x403bc3 |
| 0x67 | 957 | none | 0x404053 |
| 0x68 | 12 | none | 0x405109 |
| 0x69 | 62 | none | 0x405109 |
| 0x6b | 18922 | none | 0x40448a |
| 0x6c | 7730 | none | 0x404786 |
| 0x6d | 4344 | none | 0x4048a9 |
| 0x6e | 9686 | none | 0x404179 |
| 0x6f | 536 | none | 0x404096 |
| 0x70 | 783 | command_word | 0x403c78 |
| 0x71 | 488 | command_word | 0x403c88 |
| 0x72 | 1548 | none | 0x404f7b |
| 0x73 | 1548 | none | 0x405109 |
| 0x76 | 10732 | none | 0x405109 |
| 0x77 | 802 | none | 0x40498c |

## Native command IDs

Names are CE table labels. A handler being present does not establish v5 semantics.
Namespaces 0x4f (notify), 0x70 (collection), 0x71 (rectangle) lack v5 dispatch.

| Namespace:ID | CE label | Count | CE handler | First archive:BCOD offset |
| --- | --- | ---: | --- | --- |
| 0x4d:0x01 | eval | 1 | present | ACME Support/Cache/TrvlPick.MHK:4804 |
| 0x4d:0x02 | random | 1425 | present | ACME Support/Cache/ComBot.MHK:2383 |
| 0x4d:0x03 | stringLen | 243 | present | ACME Support/Cache/ComBot.MHK:2845 |
| 0x4d:0x04 | substring | 212 | present | ACME Support/Cache/ComBot.MHK:5142 |
| 0x4d:0x05 | max | 7 | present | ACME Support/Cache/Main.MHK:32577 |
| 0x4d:0x06 | min | 17 | present | ACME Support/Cache/Main.MHK:32559 |
| 0x4d:0x07 | abs | 8 | present | ACME Support/Cache/Main.MHK:24355 |
| 0x4d:0x08 | getRect | 22 | present | ACME Support/Cache/Main.MHK:20965 |
| 0x4d:0x09 | makePt | 220 | present | ACME Support/Cache/ComBot.MHK:370 |
| 0x4d:0x0c | mousePos | 3 | present | ACME Support/Cache/SignUp.MHK:22623 |
| 0x4d:0x11 | xpos | 35 | present | ACME Support/Cache/Main.MHK:22809 |
| 0x4d:0x12 | ypos | 37 | present | ACME Support/Cache/Main.MHK:22823 |
| 0x4d:0x14 | move | 21 | present | ACME Support/Cache/Match3.MHK:406 |
| 0x4d:0x17 | setDragParams | 4 | present | ACME Support/Cache/SignUp.MHK:22697 |
| 0x4d:0x19 | enableRollover | 23 | stub | ACME Support/Cache/Main.MHK:20678 |
| 0x4d:0x1a | setCursor | 2 | missing | ACME Support/Cache/Main.MHK:20685 |
| 0x4d:0x1f | sqrt | 1 | missing | ACME Support/Product/MHK/ZapMissle.MHK:1004 |
| 0x4d:0x20 | deleteVar | 9 | present | ACME Support/Cache/Main.MHK:3595 |
| 0x4d:0x22 | scriptLink | 289 | present | ACME Support/Cache/bgHQ.MHK:119 |
| 0x4d:0x24 | rectSect | 6 | missing | ACME Support/Product/MHK/Angler.MHK:2363 |
| 0x4d:0x3f | addToGroup | 125 | missing | ACME Support/Cache/Main.MHK:36172 |
| 0x4d:0x40 | removeFromGroup | 1 | missing | Custom Questions/MHK/cqSetup.MHK:5732 |
| 0x4d:0x41 | clearGroup | 1 | missing | ACME Support/Product/MHK/KBTag.MHK:1054 |
| 0x4d:0x42 | setPlayParams | 46 | present | ACME Support/Cache/Ambient1.MHK:72 |
| 0x4d:0x43 | autoEvent | 2142 | missing | ACME Support/Cache/ComBot.MHK:542 |
| 0x4d:0x46 | getID | 8 | missing | ACME Support/Product/MHK/Jumbler.MHK:9592 |
| 0x4d:0x47 | setCursorPosition | 1 | missing | ACME Support/Cache/Main.MHK:20736 |
| 0x4d:0x48 | getTime | 1 | missing | ACME Support/Cache/Main.MHK:24808 |
| 0x4d:0x49 | logWriteLn | 1468 | missing | ACME Support/Cache/ComBot.MHK:201 |
| 0x4d:0x51 | setHitTest | 6 | present | ACME Support/Cache/SignUp.MHK:18485 |
| 0x4d:0x52 | key | 83 | present | ACME Support/Cache/Main.MHK:21804 |
| 0x4d:0x57 | lbxCreate | 130 | present | ACME Support/Cache/ComBot.MHK:8358 |
| 0x4d:0x58 | lbxFunc | 253 | present | ACME Support/Cache/ComBot.MHK:8378 |
| 0x4d:0x59 | waitCursor | 6 | missing | ACME Support/Cache/Main.MHK:1126 |
| 0x4d:0x5b | menuItemEnable | 20 | missing | ACME Support/Cache/Main.MHK:22625 |
| 0x4d:0x5e | setPageFade | 1 | present | ACME Support/Cache/Startup.MHK:10 |
| 0x4d:0x66 | createItem | 3 | missing | ACME Support/Cache/Main.MHK:2551 |
| 0x4d:0x68 | alert | 1469 | missing | ACME Support/Cache/ComBot.MHK:256 |
| 0x4d:0x69 | getUniqueID | 48 | missing | ACME Support/Cache/Grid9.MHK:1273 |
| 0x4d:0x6b | setKeyFocus | 3 | missing | ACME Support/Cache/Main.MHK:21294 |
| 0x4d:0x6d | isItem | 390 | missing | ACME Support/Cache/ComBot.MHK:599 |
| 0x4d:0x6f | getItem  | 1 | missing | ACME Support/Product/MHK/ZapLauncher.MHK:8 |
| 0x4d:0x71 | setCascade | 6 | missing | ACME Support/Cache/OptDlog.MHK:1477 |
| 0x4d:0x74 | setRes | 969 | missing | ACME Support/Cache/Ambient1.MHK:45 |
| 0x4d:0x77 | isList | 30 | missing | ACME Support/Cache/ComBot.MHK:1174 |
| 0x4d:0x79 | setVolume | 2 | missing | ACME Support/Cache/Main.MHK:18930 |
| 0x4d:0x7a | getVolume | 2 | missing | ACME Support/Cache/Main.MHK:18792 |
| 0x4d:0x7b | pause | 29 | missing | ACME Support/Cache/Main.MHK:42110 |
| 0x4d:0x7d | setItemVolume | 4 | missing | ACME Support/Product/MHK/Decode.MHK:2057 |
| 0x4d:0x81 | unmapped | 8 | missing | ACME Support/Cache/FlapperBase.MHK:2188 |
| 0x4e:0x01 | clone | 188 | present | ACME Support/Cache/Grid9.MHK:1296 |
| 0x4e:0x09 | getFrame | 12 | missing | ACME Support/Cache/SignUp.MHK:10933 |
| 0x4e:0x0c | getText | 42 | missing | ACME Support/Cache/Grid9.MHK:4133 |
| 0x4e:0x0e | getZPrev | 2 | missing | ACME Support/Product/MHK/Quadro.MHK:2844 |
| 0x4e:0x13 | isPlaying | 94 | present | ACME Support/Cache/FlapperBase.MHK:1527 |
| 0x4e:0x14 | isVisible | 15 | missing | ACME Support/Cache/ComBot.MHK:2072 |
| 0x4e:0x15 | isLoaded | 1003 | present | ACME Support/Cache/Ambient1.MHK:29 |
| 0x4e:0x17 | load | 969 | missing | ACME Support/Cache/Ambient1.MHK:55 |
| 0x4e:0x18 | moveTo | 192 | present | ACME Support/Cache/Grid9.MHK:1347 |
| 0x4e:0x1c | seekToFrame | 171 | present | ACME Support/Cache/ComBot.MHK:2386 |
| 0x4e:0x1d | setParent | 136 | present | ACME Support/Cache/Grid9.MHK:4220 |
| 0x4e:0x1e | setZOrder | 364 | present | ACME Support/Cache/ComBot.MHK:654 |
| 0x4e:0x1f | setText | 122 | present | ACME Support/Cache/ComBot.MHK:2835 |
| 0x4e:0x22 | unloadSync | 976 | missing | ACME Support/Cache/Ambient1.MHK:34 |
| 0x4f:0x0d | unmapped | 5 | missing | ACME Support/Cache/Main.MHK:1942 |
| 0x70:0x01 | unmapped | 171 | missing | ACME Support/Cache/ComBot.MHK:4665 |
| 0x70:0x02 | unmapped | 271 | missing | ACME Support/Cache/ComBot.MHK:7645 |
| 0x70:0x03 | unmapped | 4 | missing | ACME Support/Cache/SpdMatch.MHK:1128 |
| 0x70:0x05 | unmapped | 35 | missing | ACME Support/Cache/Labeler.MHK:79 |
| 0x70:0x06 | unmapped | 44 | missing | ACME Support/Cache/FlapperBase.MHK:3240 |
| 0x70:0x07 | unmapped | 16 | missing | ACME Support/Cache/FlapperBase.MHK:4442 |
| 0x70:0x08 | unmapped | 8 | missing | ACME Support/Cache/Main.MHK:4835 |
| 0x70:0x09 | unmapped | 1 | missing | ACME Support/Cache/Main.MHK:19743 |
| 0x70:0x0a | unmapped | 9 | missing | ACME Support/Cache/Main.MHK:8097 |
| 0x70:0x0b | unmapped | 6 | missing | ACME Support/Cache/Main.MHK:6762 |
| 0x70:0x0d | unmapped | 208 | missing | ACME Support/Cache/ComBot.MHK:1339 |
| 0x70:0x0e | unmapped | 4 | missing | ACME Support/Cache/Main.MHK:27453 |
| 0x70:0x0f | unmapped | 6 | missing | ACME Support/Cache/ComBot.MHK:1186 |
| 0x71:0x01 | unmapped | 101 | missing | ACME Support/Cache/Grid9.MHK:4297 |
| 0x71:0x02 | unmapped | 93 | missing | ACME Support/Cache/Grid9.MHK:4286 |
| 0x71:0x03 | unmapped | 11 | missing | ACME Support/Cache/Main.MHK:22945 |
| 0x71:0x04 | unmapped | 14 | missing | ACME Support/Cache/Main.MHK:22971 |
| 0x71:0x05 | unmapped | 107 | missing | ACME Support/Cache/Grid9.MHK:1344 |
| 0x71:0x07 | unmapped | 72 | missing | ACME Support/Cache/Grid9.MHK:4321 |
| 0x71:0x08 | unmapped | 88 | missing | ACME Support/Cache/Grid9.MHK:4347 |
| 0x71:0x09 | unmapped | 2 | missing | ACME Support/Cache/OptDlog.MHK:1052 |

## Component constructors

| Selector | Original registered class | Sites | First archive:BCOD offset |
| --- | --- | ---: | --- |
| 1001 | datafile | 25 | ACME Support/Cache/Main.MHK:2152 |
| 1004 | font | 81 | ACME Support/Cache/ComBot.MHK:8358 |
| aaDataBase | aaDatabase | 2 | ACME Support/Cache/Main.MHK:4846 |
| aaQSelector | aaQSelector | 1 | ACME Support/Cache/Main.MHK:4874 |
| aaStringLib | aaStringLib | 4 | ACME Support/Cache/Main.MHK:24577 |
| aaUserHistory | aaUserHistory | 2 | ACME Support/Cache/Main.MHK:5356 |
| aadirectory | aaDirectory | 6 | ACME Support/Cache/Main.MHK:7527 |
| clock | clock | 4 | ACME Support/Cache/Main.MHK:25587 |
| datafile | datafile | 3 | ACME Support/Cache/SignUp.MHK:16051 |
| encryptor | encryptor | 2 | ACME Support/Cache/Setup.MHK:10532 |

## Original component method tables

Name/ID pairs are read directly from the pinned original executable. Table/class
affiliations are the initial interface map; argument, return and storage contracts
still require step-2 recovery and step-3 probes. All tables include uncalled methods.

- **aaDatabase**, table 0x496c70: 1=OpenIndex, 2=CreateIndex, 3=SetCurRecord, 4=GetRecordField, 5=GetDeck, 6=GetQUnit, 7=GetDeckList, 8=GetQUnitInfo, 9=RecordLookUp, 10=GetQuestionCount, 11=SetPaths
- **aaDirectory**, table 0x497158: 1=SetPath, 2=GetFiles, 3=SelectFolder
- **aaQSelector**, table 0x497350: 1=addUser, 2=delUser, 9=setDatabase, 3=setNumLocations, 4=setNumDecksPerLocation, 5=setNumChunksPerDeck, 6=setNumQuestionsPerChunk, 7=setNumQuestionsPerDeck, 8=setRandomSelection, 10=setLevel, 11=setSubjectList, 12=getDeck, 13=loaddecks, 14=filterdecks, 15=makeMissionList, 16=getMissionList
- **aaUserHistory**, table 0x497c80: 1=setfilepath, 2=setName, 3=getName, 4=setLevel, 5=getLevel, 6=setDeckName, 7=getDeckName, 8=getDeck, 9=loadDeck, 10=saveDeck, 11=addQuestionUnit, 12=getDeckList
- **aaStringLib**, table 0x497ab8: 1=toupper, 2=tolower, 3=stricmp
- **datafile**, table 0x497f28: 1=open, 2=close, 3=addSection, 4=buildList, 5=setCurSection, 6=getCurSection, 7=setVar, 8=loadVars, 9=deleteVar, 10=deleteCurSection, 11=deleteSection, 12=writeThruOn, 13=writeThruOff, 14=isASection, 15=getVar, 16=loadSection, 17=deleteFile, 18=openReadOnly
- **encryptor**, table 0x498260: 1=setkey, 2=getkey, 3=encrypt, 4=decrypt, 4=isValid
- **font**, table 0x4984a0: 1=setfont, 2=getfont
- **clock**, table 0x4987f0: 1=seconds, 2=minutes, 3=hours, 4=day, 5=month, 6=year, 7=dayofweek, 8=dayofyear, 9=isdst

## Static scope partition

These are asset groups, not proven user-flow reachability. Main and other-runtime
contain code shared by menus, rounds and persistence.

| Asset group | Code bytes | Native calls | Dynamic invokes | Constructors | Numeric method calls |
| --- | ---: | ---: | ---: | ---: | ---: |
| custom-editor | 20488 | 447 | 138 | 12 | 5 |
| main-core | 43376 | 829 | 167 | 19 | 40 |
| other-runtime | 379252 | 5639 | 497 | 97 | 206 |
| question-content | 404200 | 8460 | 0 | 0 | 0 |
| startup | 1244 | 28 | 0 | 2 | 2 |

All constructor, numeric-method and dynamic-call offsets are retained in
`THINKQUICK-INVENTORY.json`, together with item/event/field/script-operation counts.
Late-bound receivers and script callable names are kept distinct; no guessed
receiver type is promoted to a confirmed component requirement.
