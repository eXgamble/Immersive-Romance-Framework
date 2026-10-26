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

- **Stages:** Friend, Dating, Committed, then vanilla's Engaged and Married. A Friend is the courting phase. An Ex and a Widowed state
  follow a breakup or a death; if you killed them and someone saw it, it's Killed instead (no condolences).
  Approval (0..100, never shown) moves you between them: getting to know her, evening dates, kisses, gifts (one a day),
  fighting and clearing places together raise it. A gift can also be disliked, or an insult (a human heart).
- **Getting to know her:** "I'd like to get to know you better." (once a day, Friend to Married) opens her own
  questions (1 to 10, her add-on writes them). A question you hadn't asked gives approval; asking again does nothing.
  It's only talk: it never starts courting and never counts as straying. From Dating up, "Spend the evening with me." in the evening is a date.
- **Asking her out:** "Will you go on a date with me?" is always there for a Friend. Below askDateApproval she
  says not yet; a Friend who knows you're taken (and won't compete for you) says no; from it she says yes and names
  her place. Once she'd say yes, about half her greetings hint at it. Yes starts **the first date**: the journal says
  "Meet <her> at <her place> after sunset", she goes there and waits; come in the evening and she comes over, you
  answer "Of course I came.", it ends in a kiss and you're Dating. Not there by the end of that evening: you stood
  her up (approval, and she tells you about it next time); ask again another day. One date at a time.
- **Straying** (only once you have a partner): going on a date with someone else, kissing someone else (and
  intimate scenes, for mods that add them). Talking and gifts never count.
- **Committed has its moment:** a day after she becomes Committed she comes up to you, somewhere safe (a town, an
  inn, a home; anywhere but a dungeon while she follows you) and out of combat, and tells you how much you mean to
  her. You answer "You mean a lot to me too." and she replies. Once per relationship (again after a win-back).
- **Spouse perks:** each spouse can have her own perk (her add-on decides what it is). The framework decides when:
  on while you're married and the marriage is good, paused while she's hurt or approval is below 40, gone if she
  dies. An add-on can name an ability or perk (`spouseBonus`), condition lines on `IRF_SpouseBonusFaction 0x863`
  == 1 (a circle member's service, like Eorlund tempering your weapons), or listen for `IRF_SpouseBonus` in a
  script (Ysolda's store profits). Vanilla's own spouse perks stay as they are.
- **Adopted children** (Hearthfire's adoption, unchanged), once you're married to a candidate: their greeting tells
  you when something is wrong at home (you were away long, or the marriage is in a bad way); when all is well they
  keep vanilla's. Your spouse stays home with them: she follows you inside the family's town, but at its edge she
  comes up to you, says why she can't come and goes home; after a fast travel a message says it.
- **Gifts:** "I have something for you." opens the gift menu (gold included) for a candidate from Friend up; she
  answers as she says goodbye. Stolen and quest items can't be given, and an insult isn't taken.
- **Moods** on top of the stage: wary (you attacked her lately), hurt (broken heart), jealous (seen you with
  another candidate), strained (low approval). No mood is the happy default: her plain lines. Her
  greetings, goodbyes and idle chatter can follow them; her idles follow every stage, so a partner never falls
  back to a friend's chatter.
- **Tolerances**, per candidate: being caught straying, hitting them (a stray hit in a fight vs. turning on
  them), letting them bleed out, staying away too long. Any of them can be "never": one deed ends it for good.
- **Heartbreak**, per candidate, when you end it: she accepts it, turns bitter (won't speak to you for a while),
  or pleads with you to take her back when you talk to her.
- **Breakups** remember who ended it and why (cheating, violence, absence, bleedouts, crime, your choice). You can
  make amends after a broken heart, or try to win back an ex after a while.
- **Witnesses:** your partner catches you straying if she sees it; if someone else from her town or her family
  sees it, word may get back (as often as that witness is garrulous). Deny it (it works only with a silver
  tongue) or admit it.
- **Her family and friends** (the circle) react: a line the next time you meet (also when you adopt a child,
  married to her), and if you broke her heart they may refuse you their shop or training until you make amends.
  Now and then their everyday hellos and goodbyes are about the two of you (and your children).
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
    "wary": { "days": 5, "style": "afraid" },      // after an attack: days she's wary; "afraid" keeps away (no
                                                   // following, no dates), "angry" takes any hit as an attack,
                                                   // "cold" gives talks and dates nothing; none kisses you
    "firstDate": { "spot": "Skyrim.esm|0x08AFC4" },  // required: where she waits for your first date (a placed
                                                   // marker or furniture); "place" (a location) names it in the
                                                   // journal, by default the spot's cell's location
    "spouseBonus": "Plugin.esp|0x810",               // her own spouse perk: an ability (SPEL) or a perk you have
                                                   // while married to her and the marriage is good (optional)
    "heartbreak": { "style": "pleading", "pleads": 3, "every": 2 },  // when YOU end it: "accepting" (an ex at once),
                                                   // "bitter" (won't talk to you for "silentDays", also when she
                                                   // catches you; the win-back wait starts after), "pleading" (asks
                                                   // you to take her back when you talk to her, "pleads" times,
                                                   // "every" days apart; "No. It's over." counts one)
    "bleedoutTolerance": 6,                        // approval lost each time she goes down while following
                                                   // you (vanilla's follower slot is Protected, so she goes down
                                                   // rather than dying); nothing after the wedding
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
    "hello.dating": ["IRF - Camilla.esp|0x801"]
  },
  "circle": [
    { "name": "Lucan Valerius", "npc": "Skyrim.esm|0x01347A", "ref": "Skyrim.esm|0x013487",
      "reactions": { "amends": ["IRF - Camilla.esp|0x84D"] },
      "greetings": { "goodbye.spouse": ["IRF - Camilla.esp|0x84F"] },
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
10 disliked, 10 to 299 liked, 300 or more loved (Rules.json). One gift a day: after one, "I have something for
you." is gone until a day has passed, so nothing is given away for nothing. An insulting gift isn't taken. A candidate's community (her
town) comes from her editor location. `refuse.on` takes the events below, or `theyEnded.<why>` for only the
breakups she began for that reason (e.g. `theyEnded.violence`). `refuse.until` is `"amends"` (amends with her, or
winning her back) or a number of days (no amends follow `killed`: a refusal over it lasts for good).

**A circle member's greetings:** `greetings` keys are `hello.<set>` / `goodbye.<set>`, the sets as hers (dating,
love, marry, maramal, spouse) plus `ex` and `widowed`. The lines go in IRF_CircleHelloTopic 0x873 /
IRF_CircleGoodbyeTopic 0x874 (IRF_CircleQuest 0x872, open to any speaker; the candidates' own greeting topics take
only candidates), each with `GetIsID` of the speaker and `GetFactionRank IRF_StageFaction` run on the candidate's
reference (an OR group, == each stage of the set); add `GetRandomPercent` so their vanilla greetings still come up
(the generator: one of theirs `greetShare`, default 25, % of the time). Any line can ask about children with
`GetGlobalValue IRF_AdoptedBoys / IRF_AdoptedGirls` (the generator: `"kids": ">=1"`).

**Replies:** a line (anyone's: hers, her circle's) can get a short answer from her, said aloud right after it
(Sven: "You should have chosen me, Camilla." Camilla: "Keep dreaming, Sven."). The manifest's top-level
`"replies": [{"after": [lines], "lines": [her answers]}]`; reply n's answers go in IRF_SayTopic 0x829 with
`GetFactionRank IRF_MomentFaction == 16` and `GetFactionRank IRF_ReplyFaction (0x875) == n` (several versions:
the first that fits, e.g. by her stage). She answers only if she's within about 21 m of the speaker, alive and not
fighting; a line ended while the talk is still open gets its answer as the talk closes. Give the line a distance
condition to her (`GetDistance` < 700; the generator: `"near": {"within": 700}`) so it's only said when she can
hear it, and Say Once with 24 reset hours (the generator: `"once": "day"`) so the exchange isn't repeated.
`GetLineOfSight` between two NPCs is always false (the game only works it out when one of them is the player).

### The lines

Every line is an INFO in one of the framework's topics with these conditions, **ANDed**, run on the subject
(the speaker): `GetIsID <npc> == 1` and `GetFactionRank <faction> == <rank>` from the table. Add more
conditions of your own freely (time of day, location, quest stages). Several lines for one key without extra
conditions should be a random run (Random on each, RandomEnd on the last) or the game always plays the first; the
generator does this.

Her everyday greetings, goodbyes and idles come in five sets. Each set plays at the stages listed, and every set
that fits joins one random pick, so each stage adds lines to the last instead of replacing them:

| Set | Plays at | What it's for |
|---|---|---|
| `dating` | Dating, Committed, Engaged | the base: she's happy to see you (about 4 lines each for hello, goodbye, idle) |
| `love` | Committed, Engaged, Married | how much she loves you |
| `marry` | Committed | hints that she wants to marry you |
| `maramal` | Engaged, until the wedding is booked | reminds you to talk to Maramal |
| `spouse` | Married | married life |

So all of one topic's set lines must be ONE random run (Random on each, RandomEnd on the last), after that topic's
mood lines; the generator does this. A spouse's idle lines also carry `(IRF_StageFaction != 5 OR GetRandomPercent
< p)`, with p chosen so that about half of her idles are vanilla's spouse chatter.

| Key | Topic (Immersive Romance Framework.esp) | Faction | Rank |
|---|---|---|---|
| `court` (her opener to "I'd like to get to know you better.", once a day from Friend to Married; LinkTo her questions) (only talk: never courting, never straying) | IRF_CourtTopic 0x808 | IRF_CourtFaction 0x821 | 1 |
| `know.1` .. `know.10` (her answers to her own questions, at least one; each INFO's Prompt is the player's question. A question you hadn't asked gives approval, one a day; asking again does nothing) | IRF_KnowTopic1-10 0x864-0x86D | IRF_CandidateFaction 0x800 | 0 |
| `hello.<set>`, `goodbye.<set>`, `idle.<set>`: her everyday lines, in sets that build on each other (below). A Friend greets, says goodbye and idles as vanilla's (but see `hello.hint`) | IRF_HelloTopic 0x806 / IRF_GoodbyeTopic 0x807 / IRF_IdleTopic 0x85C | IRF_GreetFaction 0x803 (hello, goodbye) / IRF_StageFaction 0x801 (idle) | an OR group, one row `== <rank>` per stage the set plays at (2 Dating, 3 Committed, 4 Engaged, 5 Married); `maramal` also `GetQuestRunning RelationshipMarriageWedding == 0` |
| `hello.brokenheart.silent` (Goodbye flag) | IRF_HelloTopic | IRF_GreetFaction | 10 |
| `hello.brokenheart.hurt`, `goodbye.brokenheart` | Hello / Goodbye | IRF_GreetFaction | 11 |
| `hello.ex.<why>`, `goodbye.ex.<why>` | Hello / Goodbye | IRF_GreetFaction | 20 + why |
| `breakup.<why>` (her speech as she leaves you; not `choice`) | IRF_ReactionHelloTopic 0x823 | IRF_ReactionFaction 0x813 | 20 + why |
| `confront` (word got back) | IRF_ReactionHelloTopic | IRF_ReactionFaction | 30 |
| `amulet.friend` / `.dating` (turns the amulet down) | IRF_AmuletTopic 0x80D | IRF_AmuletFaction 0x80C | 1 / 3 |
| `howarewe.1`..`5` (worst to best) | IRF_HowAreWeTopic 0x80A | IRF_BandFaction 0x80F | 1..5 |
| `askdate.yes` (she names her place and time) / `.notyet` / `.taken` ("Will you go on a date with me?", a Friend) | IRF_AskDateTopic 0x82A | IRF_AskDateFaction 0x818 | 1 / 2 / 3 |
| `hello.hint`, `goodbye.hint` (she'd say yes: add `GetRandomPercent < 50` so vanilla's greetings still come up; the generator does) | Hello / Goodbye | IRF_GreetFaction | 1 |
| `firstdate` (at her spot after sunset she comes up to you; LinkTo IRF_FirstDateReplyTopic 0x871) | IRF_ForceGreetTopic 0x848 | IRF_MomentFaction | 14 |
| `firstdate.reply` (her answer to "Of course I came."; Goodbye flag: the kiss follows) | IRF_FirstDateReplyTopic 0x871 | IRF_MomentFaction | 14 |
| `firstdate.after` (after the kiss) | IRF_SayTopic 0x829 | IRF_MomentFaction | 15 |
| `stoodup` (you didn't come: at your next meeting) | IRF_ReactionHelloTopic 0x823 | IRF_ReactionFaction | 14 |
| `follow.yes` / `.no` | 0x82C | IRF_FollowAnswerFaction 0x819 | 1 / 2 |
| `kiss.yes` / `.nothere` / `.no` | 0x82E | IRF_KissFaction 0x81A | 1 / 2 / 3 |
| `breakup` (answers you ending it) | 0x830 | IRF_BreakUpFaction 0x81B | 1 |
| `amends` | 0x832 | IRF_AmendsFaction 0x81C | 1 |
| `winback.yes` / `.no` | 0x834 | IRF_WinBackFaction 0x81D | 1 / 2 |
| `secret.yes` / `.no` | 0x836 | IRF_SecretFaction 0x81E | 1 / 2 |
| `date` (her answer to "Spend the evening with me.": evenings, from Dating up, once a day) | IRF_DateTopic 0x838 | IRF_DateFaction 0x81F | 1 |
| `deny.works` / `deny.fails` | 0x83A | IRF_ConfrontFaction 0x820 | 1 / 2 |
| `admit` | 0x83C | IRF_ConfrontFaction | **GreaterThan** 0 |
| `idle.ex` (her idle chatter as an ex) | IRF_IdleTopic 0x85C | IRF_StageFaction 0x801 | 6 |
| mood versions (optional): `hello.<state>.<mood>`, `goodbye.<state>.<mood>` (strained, jealous, wary) and `idle.<stage>.<mood>` (also hurt); one more row, `GetFactionRank IRF_MoodFaction == <mood>`, and listed BEFORE the plain line (the game plays the first that fits) | as the plain line | as the plain line, plus IRF_MoodFaction 0x85B | + mood: strained 2, jealous 3, hurt 4, wary 5 |
| `plead` (a pleading ex you left asks you back; required for "pleading") | IRF_HelloTopic | IRF_GreetFaction | 30 |
| `plead.yes` / `plead.no` (her answers to "Alright. Let's try again." / "No. It's over.") | 0x85E / 0x860 | IRF_PleadFaction 0x85D | 1 |
| `stayhome` (married, with children: why she won't leave town; Goodbye flag) | IRF_ForceGreetTopic 0x848 | IRF_MomentFaction 0x816 | 8 |
| `confess` (newly Committed she comes up to you: how much you mean to her, no marriage hint; LinkTo IRF_ConfessReplyTopic 0x862) | IRF_ForceGreetTopic 0x848 | IRF_MomentFaction 0x816 | 9 |
| `confess.reply` (her answer to "You mean a lot to me too."; Goodbye flag) | IRF_ConfessReplyTopic 0x862 | IRF_MomentFaction 0x816 | 9 |
| `gift.insulted` / `.disliked` / `gift` / `.liked` / `.loved` (her answer, as her goodbye) | IRF_GiftAnswerTopic 0x846 | IRF_GiftFaction 0x841 | 1 / 2 / 3 / 4 / 5 |
| `absence.1`..`3` (back after a long time) | IRF_AbsenceHelloTopic 0x825 | IRF_AbsenceFaction 0x812 | 1..3 |
| moments (below) | IRF_SayTopic 0x829 | IRF_MomentFaction 0x816 | the moment |
| `reaction.<event>` (optional) | IRF_ReactionHelloTopic | IRF_ReactionFaction | the event |
| `recent.<event>` (optional) | IRF_RecentHelloTopic 0x827 | IRF_RecentFaction 0x814 | the event |

- **why:** 1 cheating, 2 violence, 3 absence, 4 bleedouts, 5 crime, 6 choice (you ended it).
- **events:** 2 dating, 3 committed, 4 engaged, 5 married, 6 amends, 7 playerEnded, 8 theyEnded,
  9 brokenHeart, 10 seeingSomeone (said by other candidates who know), 11 widowed, 12 wonBack, 13 killed (you
  killed them, seen), 14 stoodUp (you missed the first date), 15 adopted (married to her, you adopted a child; not for children
  already there when the framework was added). A reaction plays once, at the next meeting, within a week; a recent line plays for a while
  after the event.
- **moments:** 1 `caught`, 2 `revive`, 3 `attacked`, 8 `stayhome` (in its own topic, above), 4 `crime.approve` (required when alignment < 0),
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
yet, 0 Friend, 2 Dating, 3 Committed, 4 Engaged, 5 Married, 6 Ex, 7 Widowed, 8 Killed), `IRF_ApprovalFaction`
0x802 (0..100), `IRF_WhyFaction` 0x810 and `IRF_WhoFaction` 0x811 (1 you, 2 them) for an Ex,
`IRF_SecretFaction`, `IRF_RecentFaction` with `IRF_RecentDaysFaction` 0x815 (days since). On an adopted child,
`IRF_ChildFaction` 0x847: what their greeting is about (1-3 you were away, 11 strained, 12 bad, 13 heartbroken, 0 all
well); the framework's own lines for it live in IRF_ChildHelloTopic 0x852.

**Her own follower comments** (towns, guilds, quests, a bandit camp ahead: an expansion's worth of road talk) belong in
the add-on: its own quest and lines, triggered the vanilla way (location conditions or Story Manager events, `GetStage`,
the player's factions, Say Once). Give each of those lines one framework condition, `GetFactionRank IRF_ChatFaction`
0x86E == 1: she's following you, not hurt or silent, and no reaction, plea or confession is waiting, so a comment never
plays over a breakup. Add `IRF_StageFaction` where the words depend on what you are to her ("our home" once married).

**Adopted children** (any line, any speaker): `GetGlobalValue` of `IRF_AdoptedBoys` 0x859 and `IRF_AdoptedGirls` 0x85A,
how many boys and girls Hearthfire has you raising. Write variants of a line with these (most specific first, a plain
one last: the game plays the first that fits), e.g. "I can't leave our girl alone." for girls == 1 and boys == 0.
The generator takes them per text: `{ "text": "...", "girls": 1, "boys": ">=1" }`.

### Papyrus

`Scripts/Source/IRF.psc` (MIT): `IsCandidate`, `GetStage`, `GetApproval`, `GetPartner`, `GetWhy`, `GetWho`,
`IsBrokenHearted`, `IsIntimacyAllowed` (for mods that start scenes), `RequestStage` (story beats from your own
quests), `ModApproval`, `SetPlayerInterest`, and debug tools (`DebugSetApproval`, `DebugSkipDays`, `DebugDeed`,
`GetAdoptedBoys`, `GetAdoptedGirls`, `DebugDump`, `DebugReset`, `DebugAdopt`: a child counts as adopted without Hearthfire's move). Mod events: `IRF_StageChanged` (sender = candidate, numArg = new stage, strArg = cause),
`IRF_Strayed` (sender = the partner strayed from, numArg = 1 if caught, strArg = the deed), `IRF_SpouseBonus` (sender = the
spouse, strArg = "on" / "off", numArg = 1 / 0).

## Building

`skse/build.bat` (Visual Studio 2026 Build Tools, vcpkg, CommonLibSSE-NG v10 as a submodule). The DLL is copied
into `mod/SKSE/Plugins/`. Test add-ons live in `test/` (specs in `test/specs/`).

## License

GPL-3.0 (see LICENSE), with one additional permission: add-ons and other mods may use the framework's Papyrus
API scripts and its records (compile against them, add lines to its topics, read its factions) under any license
of their own. `mod/Source/Scripts/IRF.psc` itself is MIT.
