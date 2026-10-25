# Immersive Romance Framework

An SKSE framework for Skyrim Special Edition and Anniversary Edition that turns vanilla's friendship-to-marriage
into **courting, dating and commitment, with approval in between**, then hands over to vanilla's own Amulet of
Mara and wedding. It holds no candidates itself: add-ons register them and bring their lines.

> Work in progress (0.1.0): the core is in. See the design doc for everything planned.

## For players

- Requires [SKSE64](https://skse.silverlock.org/) and
  [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).
- Install the framework, then add-ons for the candidates you want. Candidates without an add-on stay pure
  vanilla (Friend, the Amulet of Mara, the wedding).

## For add-on authors

An add-on is a light plugin with dialogue lines, the voice files, and one JSON manifest per candidate in
`SKSE/Plugins/ImmersiveRomanceFramework/Candidates/`. No scripts are required.

```json
{
  "format": 1,
  "candidate": { "name": "Camilla Valerius", "npc": "Skyrim.esm|0x01347B", "ref": "Skyrim.esm|0x013488" },
  "attributes": { },
  "lines": {
    "court":            ["IRF - Camilla.esp|0x800"],
    "hello.courting":   ["IRF - Camilla.esp|0x801"],
    "goodbye.courting": ["IRF - Camilla.esp|0x802"]
  }
}
```

**Required lines** (a candidate missing any is not registered; the log says what is missing):

| Key | Topic (Immersive Romance Framework.esp) | Conditions every line must carry (ANDed, on the speaker) |
|---|---|---|
| `court` | `IRF_CourtTopic` 0x808 ("I'd like to get to know you better.") | `GetIsID <npc> == 1`, `GetFactionRank IRF_StageFaction (0x801) == 0` |
| `hello.<state>` | `IRF_HelloTopic` 0x806 | `GetIsID <npc> == 1`, `GetFactionRank IRF_GreetFaction (0x803) == <rank>` |
| `goodbye.<state>` | `IRF_GoodbyeTopic` 0x807 | same as hello |

States and their `IRF_GreetFaction` rank: `courting` 1, `dating` 2, `committed` 3, `engaged` 4, `spouse` 5.

**Rules**
- Never edit or re-list the framework's records; only add lines to its topics.
- The framework's record FormIDs and EditorIDs are frozen: your voice file paths contain them.

**Reading the state** (conditions on any line): `GetFactionRank` on the candidate with
`IRF_StageFaction` 0x801 (-1 not a Friend yet, 0 Friend, 1 Courting, 2 Dating, 3 Committed, 4 Engaged,
5 Married) and `IRF_ApprovalFaction` 0x802 (0..100).

**Papyrus** (`Scripts/Source/IRF.psc`): `IRF.IsCandidate`, `GetStage`, `GetApproval`, `RequestStage`,
`ModApproval`. Stage changes send the mod event `IRF_StageChanged` (sender = candidate, numArg = new stage,
strArg = cause).

## Building

`skse/build.bat` (Visual Studio 2026 Build Tools, vcpkg, CommonLibSSE-NG v10 as a submodule). The DLL is copied
into `mod/SKSE/Plugins/`.

## License

GPL-3.0 (see LICENSE), with one additional permission: add-ons and other mods may use the framework's Papyrus
API scripts and its records (compile against them, add lines to its topics, read its factions) under any license
of their own. `mod/Source/Scripts/IRF.psc` itself is MIT.
