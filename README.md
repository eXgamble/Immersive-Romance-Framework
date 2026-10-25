# Immersive Romance Framework

An SKSE framework for Skyrim Special Edition and Anniversary Edition that turns vanilla's friendship-to-marriage
into **courting, dating and commitment, with approval in between**, then hands over to vanilla's own Amulet of
Mara and wedding. It holds no candidates and no voiced lines itself: add-ons register candidates and bring every
line they say.

> Work in progress (0.1.0): every system below is built and tested in game with test add-ons. Numbers are
> placeholders until balanced.

## For players

- Requires [SKSE64](https://skse.silverlock.org/) and
  [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).
- Install the framework, then add-ons for the candidates you want. Candidates without an add-on stay pure
  vanilla (Friend, the Amulet of Mara, the wedding).
- Not compatible: Relationship Dialogue Overhaul, and other marriage mods (Marry Anyone and the like).
- Who you are interested in: `playerInterestedIn` in `SKSE/Plugins/ImmersiveRomanceFramework/Rules.json`
  (`any`, `male`, `female`). Every other number lives there too, commented.

### What it does

- **Stages:** Friend, Courting, Dating, Committed, then vanilla's Engaged and Married. An Ex and a Widowed state
  follow a breakup or a death; if you killed them and someone saw it, it's Killed instead (no condolences).
  Approval (0..100, never shown) moves you between them: talks, evening dates, kisses, gifts (one a day counts),
  fighting and clearing places together raise it. A gift can also be disliked, or an insult (a human heart).
- **Gifts:** "I have something for you." opens the gift menu (gold included) for a candidate from Friend up; she
  answers as she says goodbye. Stolen and quest items can't be given, and an insult isn't taken.
- **Tolerances**, per candidate: being caught straying, hitting them (a stray hit in a fight vs. turning on
  them), letting them bleed out, staying away too long. Any of them can be "never": one deed ends it for good.
- **Breakups** remember who ended it and why (cheating, violence, absence, bleedouts, crime, your choice). You can
  make amends after a broken heart, or try to win back an ex after a while.
- **Witnesses:** your partner catches you straying if she sees it; if someone else from her town or her family
  sees it, word may get back (as often as that witness is garrulous). Deny it (it works only with a silver
  tongue) or admit it.
- **Her family and friends** (the circle) react: a line the next time you meet, and if you broke her heart they
  may refuse you their shop or training until you make amends.
- **Crimes:** a candidate who sees you steal, pickpocket, assault or murder judges it by her alignment (good ones
  disapprove, evil ones approve), a cheap theft included.
- **Beside you:** a partner who follows you fights to her own profile (role, nerve, gear) and grows in level up to
  her cap. She can die if you don't get her up in time; an essential one is only knocked out.
- **On the road:** lines in ruins, caves, towns and inns, after a fight, a dragon, a rest; a jealous line when
  she sees you travelling with another candidate.

## For add-on authors

An add-on is one plugin (ESL-flagged) with the dialogue lines and their voice files, plus one JSON manifest per
candidate in `SKSE/Plugins/ImmersiveRomanceFramework/Candidates/`. No scripts are required. The generator
`tools/irf_addon.py` turns a short spec into the records and the manifest (see its header).

A candidate whose manifest is missing a required line, or whose lines don't carry the right conditions, is
**rejected**: not registered, stays vanilla, and the log says exactly what is wrong.

### The manifest

```jsonc
{
  "format": 1,
  "candidate": { "name": "Camilla Valerius", "npc": "Skyrim.esm|0x01347B", "ref": "Skyrim.esm|0x013488" },
  "attributes": {
    "cheatTolerance": 30,                          // approval lost when caught; "never" = one deed ends it forever
    "hitTolerance": { "cost": 3, "strayHits": 3, "lockDays": 2 },   // per stray hit; hits per fight that count
                                                   // as an attack; days she won't follow or date after one
    "bleedoutTolerance": 6,                        // per bleedout
    "absenceTolerance": { "days": 5, "cost": 4 },  // apart longer than days costs cost per band (1-3)
    "brokenHeartDays": 2,                          // silent this long after catching you, then hurt until amends
    "modesty": 1,                                  // 0 kisses anywhere .. 3 only at home, alone
    "scruples": "refuse",                          // someone else's partner: "refuse", "stayout" or "compete"
    "garrulous": 40,                               // chance (%) she passes on what she saw
    "alignment": 2,                                // -5 evil .. 0 gray .. 5 good: how she judges your crimes
    "interestedIn": "any",                         // "any", "male", "female"
    "refusesSecret": false,                        // won't keep the relationship secret
    "gifts": {                                     // an item, a keyword it carries, or a form list holding it
      "insulted": [], "disliked": ["Skyrim.esm|0x03AD6F"],
      "liked": ["Skyrim.esm|0x08F95A"], "loved": ["Skyrim.esm|0x064B3D"]
    },
    "combat": { "role": "support", "combatStyle": "Plugin.esp|0x800", "confidence": 2, "aggression": 1,
                "gear": ["Skyrim.esm|0x012EB7"], "levelCap": 10 },
    "gates": { "dating": { "quest": "Plugin.esp|0x801", "stage": 50 } }   // Dating (or "committed") waits for
                                                                          // this quest stage
  },
  "lines": {
    "court": ["IRF - Camilla.esp|0x800"],
    "hello.courting": ["IRF - Camilla.esp|0x801"]
  },
  "circle": [
    { "name": "Lucan Valerius", "npc": "Skyrim.esm|0x01347A", "ref": "Skyrim.esm|0x013487",
      "reactions": { "amends": ["IRF - Camilla.esp|0x84D"] },
      "refuse": { "on": ["brokenHeart"], "until": "amends", "services": ["trade"],
                  "lines": ["IRF - Camilla.esp|0x84E"] } }
  ]
}
```

Every attribute is optional; a missing one takes the default from Rules.json.

**Gifts:** a candidate checks her own lists first (an item named outright, then a keyword or form list, the
strongest objection first), then the framework's keyword `IRF_GiftInsulting` (0x83F): items the average person
takes as an insult (a human heart, human flesh, a skull). The framework gives them the keyword at load from
`SKSE/Plugins/ImmersiveRomanceFramework/Gifts/*.json`; another mod can add its own file there or give its items the
keyword. A candidate who doesn't mind them lists the keyword under `liked` or `loved`. Gold goes by the amount: under
10 disliked, 10 to 299 liked, 300 or more loved (Rules.json). One liked, loved or plain gift a day counts; every
disliked or insulting one does, and an insulting one isn't taken. A candidate's community (her
town) comes from her editor location. `refuse.until` is `"amends"` or a number of days (no amends
follow `killed`: a refusal over it lasts for good).

### The lines

Every line is an INFO in one of the framework's topics with these conditions, **ANDed**, run on the subject
(the speaker): `GetIsID <npc> == 1` and `GetFactionRank <faction> == <rank>` from the table. Add more
conditions of your own freely (time of day, location, quest stages).

| Key | Topic (Immersive Romance Framework.esp) | Faction | Rank |
|---|---|---|---|
| `court` / `court.taken` | IRF_CourtTopic 0x808 | IRF_TakenFaction 0x821 | 1 / 2 (declines: taken) |
| `hello.<state>`, `goodbye.<state>` | IRF_HelloTopic 0x806 / IRF_GoodbyeTopic 0x807 | IRF_GreetFaction 0x803 | courting 1, dating 2, committed 3, engaged 4, spouse 5 |
| `hello.brokenheart.silent` (Goodbye flag) | IRF_HelloTopic | IRF_GreetFaction | 10 |
| `hello.brokenheart.hurt`, `goodbye.brokenheart` | Hello / Goodbye | IRF_GreetFaction | 11 |
| `hello.ex.<why>`, `goodbye.ex.<why>` | Hello / Goodbye | IRF_GreetFaction | 20 + why |
| `breakup.<why>` (her speech as she leaves you; not `choice`) | IRF_ReactionHelloTopic 0x823 | IRF_ReactionFaction 0x813 | 20 + why |
| `confront` (word got back) | IRF_ReactionHelloTopic | IRF_ReactionFaction | 30 |
| `amulet.friend` / `.courting` / `.dating` (turns the amulet down) | IRF_AmuletTopic 0x80D | IRF_AmuletFaction 0x80C | 1 / 2 / 3 |
| `howarewe.1`..`5` (worst to best) | IRF_HowAreWeTopic 0x80A | IRF_BandFaction 0x80F | 1..5 |
| `askdate.yes` / `.notyet` | 0x82A | IRF_AskDateFaction 0x818 | 1 / 2 |
| `follow.yes` / `.no` | 0x82C | IRF_FollowAnswerFaction 0x819 | 1 / 2 |
| `kiss.yes` / `.nothere` / `.no` | 0x82E | IRF_KissFaction 0x81A | 1 / 2 / 3 |
| `breakup` (answers you ending it) | 0x830 | IRF_BreakUpFaction 0x81B | 1 |
| `amends` | 0x832 | IRF_AmendsFaction 0x81C | 1 |
| `winback.yes` / `.no` | 0x834 | IRF_WinBackFaction 0x81D | 1 / 2 |
| `secret.yes` / `.no` | 0x836 | IRF_SecretFaction 0x81E | 1 / 2 |
| `talk` (a talk; in the evening it's a date) | 0x838 | IRF_TalkFaction 0x81F | 1 |
| `deny.works` / `deny.fails` | 0x83A | IRF_ConfrontFaction 0x820 | 1 / 2 |
| `admit` | 0x83C | IRF_ConfrontFaction | **GreaterThan** 0 |
| `gift.insulted` / `.disliked` / `gift` / `.liked` / `.loved` (her answer, as her goodbye) | IRF_GiftAnswerTopic 0x846 | IRF_GiftFaction 0x841 | 1 / 2 / 3 / 4 / 5 |
| `absence.1`..`3` (back after a long time) | IRF_AbsenceHelloTopic 0x825 | IRF_AbsenceFaction 0x812 | 1..3 |
| moments (below) | IRF_SayTopic 0x829 | IRF_MomentFaction 0x816 | the moment |
| `reaction.<event>` (optional) | IRF_ReactionHelloTopic | IRF_ReactionFaction | the event |
| `recent.<event>` (optional) | IRF_RecentHelloTopic 0x827 | IRF_RecentFaction 0x814 | the event |

- **why:** 1 cheating, 2 violence, 3 absence, 4 bleedouts, 5 crime, 6 choice (you ended it).
- **events:** 1 courting, 2 dating, 3 committed, 4 engaged, 5 married, 6 amends, 7 playerEnded, 8 theyEnded,
  9 brokenHeart, 10 seeingSomeone (said by other candidates who know), 11 widowed, 12 wonBack, 13 killed (you
  killed them, seen). A reaction plays once, at the next meeting, within a week; a recent line plays for a while
  after the event.
- **moments:** 1 `caught`, 2 `revive`, 3 `attacked`, 4 `crime.approve` (required when alignment < 0),
  5 `crime.disapprove` (when alignment > 0), 6 `jealous`, 10 `road.afterfight`, 11 `road.dragon`,
  12 `road.nearlydead`, 13 `road.rest`, 20 `road.nordicruin`, 21 `road.dwarvenruin`, 22 `road.cave`,
  23 `road.dungeon`, 24 `road.town`, 25 `road.inn`, 26 `road.hometown`.
- **Circle lines** take the member's `GetIsID`: reactions in IRF_ReactionHelloTopic by event, the refusal in
  IRF_SayTopic with moment 7.

### Rules

- Never edit or re-list the framework's records; only add lines to its topics.
- The framework's record FormIDs and EditorIDs are frozen: your voice file paths contain them.
- Your plugin carries a copy of every framework topic it adds lines to (the game needs the topic in the plugin
  that adds lines to it), and the last-loaded copy wins the topic's own fields. Keep the copies identical to the
  framework's (the generator does) and regenerate when a framework update changes a topic.

### Reading the state

Conditions on any line, run on the candidate: `GetFactionRank` with `IRF_StageFaction` 0x801 (-1 not a Friend
yet, 0 Friend, 1 Courting, 2 Dating, 3 Committed, 4 Engaged, 5 Married, 6 Ex, 7 Widowed, 8 Killed), `IRF_ApprovalFaction`
0x802 (0..100), `IRF_WhyFaction` 0x810 and `IRF_WhoFaction` 0x811 (1 you, 2 them) for an Ex,
`IRF_SecretFaction`, `IRF_RecentFaction` with `IRF_RecentDaysFaction` 0x815 (days since).

### Papyrus

`Scripts/Source/IRF.psc` (MIT): `IsCandidate`, `GetStage`, `GetApproval`, `GetPartner`, `GetWhy`, `GetWho`,
`IsBrokenHearted`, `IsIntimacyAllowed` (for mods that start scenes), `RequestStage` (story beats from your own
quests), `ModApproval`, `SetPlayerInterest`, and debug tools (`DebugSetApproval`, `DebugSkipDays`, `DebugDeed`,
`DebugDump`, `DebugReset`). Mod events: `IRF_StageChanged` (sender = candidate, numArg = new stage, strArg = cause),
`IRF_Strayed` (sender = the partner strayed from, numArg = 1 if caught, strArg = the deed).

## Building

`skse/build.bat` (Visual Studio 2026 Build Tools, vcpkg, CommonLibSSE-NG v10 as a submodule). The DLL is copied
into `mod/SKSE/Plugins/`. Test add-ons live in `test/` (specs in `test/specs/`).

## License

GPL-3.0 (see LICENSE), with one additional permission: add-ons and other mods may use the framework's Papyrus
API scripts and its records (compile against them, add lines to its topics, read its factions) under any license
of their own. `mod/Source/Scripts/IRF.psc` itself is MIT.
