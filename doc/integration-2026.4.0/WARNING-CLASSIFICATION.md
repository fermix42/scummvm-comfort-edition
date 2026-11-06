# Original Release warning classification

Count original diagnostics before Build succeeded; exclude repeated summary and template-context lines. Provenance requires matching historical diagnostics with identical CE source, or successful targeted compilation of the recorded baseline/tag source with Release flags. Toolchain/vendor diagnostics also retain their source provenance.

| Source location | Code | Occurrences | Classification | Evidence |
| --- | --- | ---: | --- | --- |
| engines/director/castmember/xtra.cpp:130:2 | C4065 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/castmember/xtra.cpp:139:2 | C4065 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| backends/imgui/components/imgui_memory_editor.h:578:38 | C4775 | 15 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/debugger/dt-save-state.cpp:166:29 | C4334 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:476:21 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:482:52 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:489:18 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:489:53 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:502:18 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/director/transitions.cpp:521:18 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/eem/graphics.cpp:480:52 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/eem/ui.cpp:4465:26 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/eem/ui.cpp:4859:27 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/eem/ui.cpp:4986:29 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/fool/fool_game.cpp:1127:26 | C4245 | 1 | Existing upstream warning | warning-repro-fool.log |
| engines/freescape/games/3dck/8bitUI.cpp:173:21 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/freescape/games/3dck/c64.cpp:59:37 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/freescape/games/castle/c64.cpp:488:98 | C4018 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/phoenixvr/phoenixvr.cpp:742:4 | C4065 | 1 | Existing upstream warning | warning-repro-phoenixvr.log |
| engines/phoenixvr/phoenixvr.cpp:768:4 | C4065 | 1 | Existing upstream warning | warning-repro-phoenixvr.log |
| engines/sci/graphics/screen.cpp:234:15 | C4018 | 1 | Existing CE warning | warning-repro-sci.log |
| engines/sci/dialogs.cpp:907:44 | C4189 | 1 | Existing CE warning | warning-repro-sci.log |
| engines/scumm/insane/rebel2/render.cpp:161:10 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/teenagent/scene.cpp:552:3 | C4065 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/tinsel/noir/spriter.cpp:376:11 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/tinsel/noir/spriter.cpp:380:11 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/twp/squirrel/sqstdrex.cpp:568:8 | C4611 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| engines/twp/squirrel/sqcompiler.cpp:174:12 | C4611 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| common/singleton.h:59:12 | C4506 | 18 | Existing upstream warning | mi4-before-controller-build.log |
| backends/graphics/opengl/renderer3d.cpp:367:31 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| backends/graphics/opengl/renderer3d.cpp:368:17 | C4146 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| common/lua/ldo.cpp:141:3 | C4611 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| common/lua/lgc.cpp:286:45 | C4334 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| common/lua/ltable.cpp:324:5 | C4334 | 1 | Existing upstream warning | mi4-before-controller-build.log |
| common/lua/ltable.cpp:378:5 | C4334 | 1 | Existing upstream warning | mi4-before-controller-build.log |

Total: 66 occurrences across 35 locations.

{"Existing CE warning": 2, "Existing upstream warning": 64}
