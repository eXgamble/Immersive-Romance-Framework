"""Immersive Romance Framework: add-on generator.

Turns a short spec into everything an add-on needs:
  - <out>/records.json : the dialogue lines as houseCARL create records (housecarl_create records="@...")
  - <out>/manifest.json: the candidate manifest, with every line's FormID filled in

Every line gets the conditions the framework's checker requires (GetIsID of the speaker + the right
GetFactionRank row) and sits in the right framework topic. Line FormIDs are allocated from 0x800 in the order
records.json lists them, which is how houseCARL allocates them in a new plugin, so the manifest matches.

Spec (JSON):
{
  "plugin": "IRF - Camilla.esp",
  "candidate": { "name": "Camilla Valerius", "npc": "Skyrim.esm|0x01347B", "ref": "Skyrim.esm|0x013488" },
  "attributes": { ... },                      copied into the manifest as is
  "lines": { "court": "Text" | ["Text", ...], ... },   any key missing gets a placeholder ("[Name: key]");
             several plain texts for a key are a random pick
             a text may be { "text": "...", "boys": 0 | ">=1", "girls": ... }: played only with that many adopted
             boys / girls (IRF_AdoptedBoys / IRF_AdoptedGirls); the game plays the first that fits, so list the
             most specific first and a plain one last
             a text may also carry "alive": "Skyrim.esm|0x013487" or "dead": ... (a placed reference): played only
             while that person lives (or after they died), e.g. lines about her brother; these join the random pick
             a text's "text" may be a list: one line said as several responses in a row (a longer answer)
             a text may carry "rank": "<0" (the speaker's relationship rank with you; "<0" = your rival) or
             {"toward": "Skyrim.esm|0x013491", "is": "<0"} (with someone else), and "stageDone": "Skyrim.esm|0x074A83:310"
             (that quest stage is done); give the other texts of that key the opposite, or (circle reactions, which
             play the first that fits) list the plain one last
             a text may carry "kids": ">=1" (you're raising at least one adopted child) or "kids": 0 (none)
             "following": true / false (she's following you or not), "near": {"who": ref (default: her),
             "within": 700, "sees": true} (the speaker is that close to them; "sees": in their line of sight, which
             the game only works out when one of the two is the player: never between two NPCs), "once": "day" (Say
             Once, again after 24 hours), "set": "dating" (the speaker's own stage set: for her replies),
             "happy": true / false (she's happy with you: her IRF_BandFaction 4 or 5, as her "I'm happy", and no mood)
             "player": "male" / "female" (the player's sex: for the few lines that name it; give the other sex its own text)
             "jealousOf": "fighter" / "nonfighter" / "both" (the woman she got jealous of today: a fighter at your side,
             or not; for her "jealous" line and her jealous-mood lines; "both", the default, plays for either). The
             condition doesn't change a line's words, so marking voiced lines keeps their FormIDs: no new line has to
             come first
             a text may carry "reply": "Text" | [{"text": ..., "set": "spouse"}, ...]: she answers it aloud right after
             (the first version that fits); give the line "near" her so she's there to hear it
  two partners having it out (optional, both add-ons need them for the scene): "expose.ask", "expose.claim.<dating|
             committed|engaged|spouse>", "expose.counter.<same>", "expose.question", "expose.believed",
             "expose.disbelieved", "expose.disowned", "expose.true" (and "expose.reply.<outcome>" when her answer in the
             talk should differ from her line after it)
  "family": [ {"when": {"home": "happy|fine|strained|bad|heartbroken" (or a list), "mood": "jealous|wary|...",
                         "alive": "Skyrim.esm|0x013487", "dead": ...}, "child": "boy|girl",
              "lines": [{"child": "Text"}, {"her": "Text"}, ...]} ]   her conversations with the adopted children, when
             (a line may be versions: {"her": [{"text": ..., "player": "male"}, {"text": ..., "player": "female"}]})
             home is that way (her heart and approval) and, given, she's in that mood; either may start, any number of
             turns; the framework picks one a day (the least recent that fits) and stages it
  "children": {"away1|away2|away3|strained|bad|heartbroken": "Text" | [...]}   the adopted children's greeting when
             something is wrong at home (the framework says when; you're married to her): both child voices
  "placeholders": true,                      fill every required key that has no text
  "records": [ {houseCARL create record}, ... ]  her own other records (a spouse perk, ...), created FIRST so their
                                             FormIDs never move; an attribute value "@EditorID" names one of them
  "released": "1.0"                          once the add-on is out: its FormIDs are frozen (other mods' patches,
                                             translations and voice replacers point at them), so --relock is refused
                                             unless --major says this build is a new major version (a breaking release)
  "lock": "camilla.ids.json"                 (beside the spec) every record's FormID, for good: voice files are named
                                             by FormID. A line is known by its spoken words (its EditorID ends in a
                                             hash of them): the same words keep their FormID wherever the line moves;
                                             new or reworded lines take the next free FormIDs; a removed line's FormID
                                             is kept by a placeholder global and never reused. Each line's PNAM
                                             (previous line) puts it in its place in the topic: the game slots a line
                                             after its PNAM when that line loaded before it (measured 2026-10-26)
  "circle": [ { "name": "Lucan", "npc": "...", "ref": "...",
                "reactions": { "amends": "Text" },
                "greetings": { "goodbye.spouse": ["Text", {"text": "...", "kids": ">=1"}] },   their everyday hellos
                       and goodbyes while she is at that stage with you (sets as hers: dating, love, marry, maramal,
                       spouse, and ex, widowed); mixed with their vanilla ones: one of theirs plays "greetShare"
                       (default 25) % of the time
                "refuse": { "on": ["brokenHeart"], "until": "amends", "services": ["trade"], "lines": "Text" } } ]
}

Usage: python irf_addon.py spec.json out_folder [--relock] [--into Build.esp]
  out_folder gets records.json (create them, in that order, into a new empty ESL), order.json (then apply it there:
  each line's PNAM) and manifest.json. --into names the plugin they're built in when that isn't the spec's (the
  FormIDs in order.json name it). --relock numbers everything afresh: only while no voice files exist.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

FRAMEWORK = "Immersive Romance Framework.esp"

# topics (Immersive Romance Framework.esp)
HELLO, GOODBYE, COURT, HOWAREWE, AMULET = 0x806, 0x807, 0x808, 0x80A, 0x80D
REACTION_HELLO, ABSENCE_HELLO, RECENT_HELLO, SAY = 0x823, 0x825, 0x827, 0x829
GIFT_ANSWER = 0x846  # her goodbye after a gift
FORCEGREET = 0x848  # IRF_ForceGreetTopic: what she comes up to tell you (stay home, confess)
CONFESS_REPLY = 0x862  # IRF_ConfessReplyTopic: "You mean a lot to me too."
KNOW, KNOW_MAX = 0x864, 10  # IRF_KnowTopic1-10 (0x864-0x86D): the get-to-know questions (at least one)
CANDIDATE_F = 0x800  # IRF_CandidateFaction: every registered candidate, rank 0
JEALOUS_OF_F = 0x876  # IRF_JealousOfFaction: who she got jealous of today: 1 a fighter, 2 not
REPLY_F, REPLY_MOMENT = 0x875, 16  # her answer to someone's line: IRF_ReplyFaction == its number, in IRF_SayTopic
HER = [None]  # her reference (houseCARL form), for conditions about her
FIRSTDATE_REPLY = 0x871  # IRF_FirstDateReplyTopic: "Of course I came."
CIRCLE_HELLO, CIRCLE_GOODBYE = 0x873, 0x874  # a circle member's everyday greetings (IRF_CircleQuest: any speaker)
# two partners have it out (the framework's Expose): moments, the outcome faction, your three answers to her question
EXPOSE_ASK, EXPOSE_CLAIM, EXPOSE_COUNTER, EXPOSE_QUESTION, EXPOSE_OUTCOME = 27, 28, 29, 30, 31
EXPOSE_F = 0x877  # IRF_ExposeFaction: 1 she believes you, 2 she doesn't, 3 you denied her, 4 you told the truth
EXPOSE_SIDE_A, EXPOSE_SIDE_B, EXPOSE_TRUE = 0x878, 0x879, 0x87A
EXPOSE_OUTCOMES = ["believed", "disbelieved", "disowned", "true"]
# her family conversations with the adopted children (the framework's Family): moment, which one (on her), which line
FAMILY_MOMENT, FAMILY_F, FAMILY_STEP_F = 32, 0x87B, 0x87C
FAMILY_HOMES = ["happy", "fine", "strained", "bad", "heartbroken"]
# the children's greetings when something is wrong at home: IRF_ChildHelloTopic, IRF_ChildFaction on the child (the
# framework sets when); married to her
CHILD_HELLO, CHILD_F = 0x852, 0x847
CHILD_STATES = {"away1": 1, "away2": 2, "away3": 3, "strained": 11, "bad": 12, "heartbroken": 13}
LINKS = {"confess": [CONFESS_REPLY], "firstdate": [FIRSTDATE_REPLY], "expose.question": [EXPOSE_SIDE_A, EXPOSE_SIDE_B, EXPOSE_TRUE]}  # the replies her line offers (LinkTo); "court" links to her questions (main)
ASKDATE, FOLLOW, KISS, BREAKUP, AMENDS, WINBACK, SECRET, DATE, DENY, ADMIT = (
    0x82A, 0x82C, 0x82E, 0x830, 0x832, 0x834, 0x836, 0x838, 0x83A, 0x83C)
# factions
STAGE_F, GREET_F, AMULET_F, BAND_F = 0x801, 0x803, 0x80C, 0x80F
ABSENCE_F, REACTION_F, RECENT_F, MOMENT_F = 0x812, 0x813, 0x814, 0x816
ASKDATE_F, FOLLOW_F, KISS_F, BREAKUP_F, AMENDS_F, WINBACK_F, SECRET_F, DATE_F, CONFRONT_F, COURT_F = (
    0x818, 0x819, 0x81A, 0x81B, 0x81C, 0x81D, 0x81E, 0x81F, 0x820, 0x821)
GIFT_F = 0x841  # her answer to a gift: 1 insulted, 2 disliked, 3 plain, 4 liked, 5 loved
MOOD_F, IDLE = 0x85B, 0x85C  # her mood (IRF_MoodFaction), her idles (IRF_IdleTopic, by IRF_StageFaction)
MOODS = {"strained": 2, "jealous": 3, "hurt": 4, "wary": 5}
PLEAD_F, PLEAD_YES, PLEAD_NO, PLEAD_GREET = 0x85D, 0x85E, 0x860, 30  # a pleading ex you left
SPOUSE_IDLE_SHARE = 50  # a spouse's own idles play this % of the time: vanilla's spouse chatter mixes in
WEDDING = "07404E:Skyrim.esm"  # RelationshipMarriageWedding: runs once the wedding is booked
# her everyday greetings, goodbyes and idles: sets that build on each other (user, 2026-10-25). Each set plays at
# the stages listed (IRF_GreetFaction / IRF_StageFaction 2 Dating, 3 Committed, 4 Engaged, 5 Married) and every
# set that fits joins ONE random pick per topic. "maramal" stops once the wedding is booked.
SETS = {"dating": [2, 3, 4], "love": [3, 4, 5], "marry": [3], "maramal": [4], "spouse": [5]}
CIRCLE_SETS = {**SETS, "ex": [6], "widowed": [7]}  # a circle member's greetings: hers, and after it ended
CIRCLE_GREET_SHARE = 25
HAPPY_BAND = 4  # IRF_BandFaction from here up: she's happy with you (her "howarewe.4": "I'm happy.")
EXCHANGE_SHARE = 50  # a line she answers (with "reply"): this % of the time when it fits
MARRIED = 5
ADOPTED = {"boys": 0x859, "girls": 0x85A}  # IRF_AdoptedBoys / IRF_AdoptedGirls (globals)
OPS = {"==": "EqualTo", "!=": "NotEqualTo", ">=": "GreaterThanOrEqualTo", "<=": "LessThanOrEqualTo", ">": "GreaterThan", "<": "LessThan"}

EVENTS = {"dating": 2, "committed": 3, "engaged": 4, "married": 5, "amends": 6,
          "playerEnded": 7, "theyEnded": 8, "brokenHeart": 9, "seeingSomeone": 10, "widowed": 11, "wonBack": 12, "killed": 13,
          "stoodUp": 14, "adopted": 15}
WHYS = {"cheating": 1, "violence": 2, "absence": 3, "bleedouts": 4, "crime": 5, "choice": 6}
REFUSE_MOMENT = 7  # a circle member refusing service
STAYHOME_MOMENT = 8  # a spouse with children won't leave town
CONFESS_MOMENT = 9  # newly Committed: she tells you how much you mean to her
FIRSTDATE_MOMENT, FIRSTDATE_AFTER = 14, 15  # the first date at her spot, and her line after the kiss
HINT_GREET, HINT_SHARE = 1, 50  # she'd say yes to a date: her hint hello/goodbye, this % of the time (else vanilla's)
MOMENTS = {"caught": 1, "revive": 2, "attacked": 3, "crime.approve": 4, "crime.disapprove": 5, "jealous": 6,
           "road.afterfight": 10, "road.dragon": 11, "road.nearlydead": 12, "road.rest": 13,
           "road.nordicruin": 20, "road.dwarvenruin": 21, "road.cave": 22, "road.dungeon": 23,
           "road.town": 24, "road.inn": 25, "road.hometown": 26}


def requirements(attributes):
    """key -> (topic, faction, rank, operator, goodbye flag, required, extra condition rows); mirrors Candidates.cpp.
    rank may be a list (a set: an OR group, one row per stage). Mood versions come before the sets: the game plays
    the first line that fits."""
    r = {}
    alignment = attributes.get("alignment", 0)
    pleading = attributes.get("heartbreak", {}).get("style") == "pleading"

    def add(key, topic, faction, rank, required=True, op="EqualTo", goodbye=False, extra=None):
        r[key] = (topic, faction, rank, op, goodbye, required, extra or [])

    def mood_row(mood):
        return condition("GetFactionRankConditionData", "Faction", fw(MOOD_F), MOODS[mood], "EqualTo")

    # "I'd like to get to know you better.": her opener links to her questions (only talk, never courting)
    add("court", COURT, COURT_F, 1)
    for i in range(1, KNOW_MAX + 1):  # her answers, at least one: {"prompt": "<your question>", "text": "<her answer>"}
        add(f"know.{i}", KNOW + i - 1, CANDIDATE_F, 0, required=i == 1)
    # Courting greets, says goodbye and idles as vanilla's Friend: her own everyday lines start at Dating.
    # Mood versions first (the game plays the first line that fits; hurt: idles only, the broken heart greets)
    for rank, name in enumerate(["dating", "committed", "engaged", "spouse"], start=2):
        for mood in MOODS:
            if mood != "hurt":
                add(f"hello.{name}.{mood}", HELLO, GREET_F, rank, required=False, extra=[mood_row(mood)])
                add(f"goodbye.{name}.{mood}", GOODBYE, GREET_F, rank, required=False, extra=[mood_row(mood)])
            add(f"idle.{name}.{mood}", IDLE, STAGE_F, rank, required=False, extra=[mood_row(mood)])
    # then the sets (main() emits each topic's sets as one random run)
    for name, ranks in SETS.items():
        booked = [condition("GetQuestRunningConditionData", "Quest", WEDDING, 0)] if name == "maramal" else []
        add(f"hello.{name}", HELLO, GREET_F, ranks, extra=booked)
        add(f"goodbye.{name}", GOODBYE, GREET_F, ranks, extra=booked)
        add(f"idle.{name}", IDLE, STAGE_F, ranks, extra=booked)
    add("idle.ex", IDLE, STAGE_F, 6)
    # a pleading candidate you left: her asking (a greeting), her answers to "Let's try again." / "It's over."
    add("plead", HELLO, GREET_F, PLEAD_GREET, required=pleading)
    add("plead.yes", PLEAD_YES, PLEAD_F, 1, required=pleading)
    add("plead.no", PLEAD_NO, PLEAD_F, 1, required=pleading)
    add("hello.brokenheart.silent", HELLO, GREET_F, 10, goodbye=True)
    add("hello.brokenheart.hurt", HELLO, GREET_F, 11)
    add("goodbye.brokenheart", GOODBYE, GREET_F, 11)
    for name, why in WHYS.items():
        add(f"hello.ex.{name}", HELLO, GREET_F, 20 + why)
        add(f"goodbye.ex.{name}", GOODBYE, GREET_F, 20 + why)
        if name != "choice":
            add(f"breakup.{name}", REACTION_HELLO, REACTION_F, 20 + why)
    add("amulet.friend", AMULET, AMULET_F, 1)
    add("amulet.dating", AMULET, AMULET_F, 3)
    for band in range(1, 6):
        add(f"howarewe.{band}", HOWAREWE, BAND_F, band)
    add("askdate.yes", ASKDATE, ASKDATE_F, 1)
    add("askdate.notyet", ASKDATE, ASKDATE_F, 2)
    add("askdate.taken", ASKDATE, ASKDATE_F, 3)  # a Friend who knows you're taken (and won't compete)
    # once she'd say yes: a hello and a goodbye that hint at it, mixed with her vanilla ones
    add("hello.hint", HELLO, GREET_F, HINT_GREET, extra=[random_row(HINT_SHARE)])
    add("goodbye.hint", GOODBYE, GREET_F, HINT_GREET, extra=[random_row(HINT_SHARE)])
    add("follow.yes", FOLLOW, FOLLOW_F, 1)
    add("follow.no", FOLLOW, FOLLOW_F, 2)
    add("kiss.yes", KISS, KISS_F, 1)
    add("kiss.nothere", KISS, KISS_F, 2)
    add("kiss.no", KISS, KISS_F, 3)
    add("breakup", BREAKUP, BREAKUP_F, 1)
    add("amends", AMENDS, AMENDS_F, 1)
    add("winback.yes", WINBACK, WINBACK_F, 1)
    add("winback.no", WINBACK, WINBACK_F, 2)
    add("secret.yes", SECRET, SECRET_F, 1)
    add("secret.no", SECRET, SECRET_F, 2)
    add("date", DATE, DATE_F, 1)
    add("confront", REACTION_HELLO, REACTION_F, 30)
    add("deny.works", DENY, CONFRONT_F, 1)
    add("deny.fails", DENY, CONFRONT_F, 2)
    add("admit", ADMIT, CONFRONT_F, 0, op="GreaterThan")
    for band in range(1, 4):
        add(f"absence.{band}", ABSENCE_HELLO, ABSENCE_F, band)
    add("stayhome", FORCEGREET, MOMENT_F, STAYHOME_MOMENT, goodbye=True)
    add("confess", FORCEGREET, MOMENT_F, CONFESS_MOMENT)
    add("confess.reply", CONFESS_REPLY, MOMENT_F, CONFESS_MOMENT, goodbye=True)
    # the first date: she comes up to you at her spot, answers "Of course I came.", her line after the kiss; and
    # if you never came, her line at your next meeting
    add("firstdate", FORCEGREET, MOMENT_F, FIRSTDATE_MOMENT)
    add("firstdate.reply", FIRSTDATE_REPLY, MOMENT_F, FIRSTDATE_MOMENT, goodbye=True)
    add("firstdate.after", SAY, MOMENT_F, FIRSTDATE_AFTER)
    add("stoodup", REACTION_HELLO, REACTION_F, EVENTS["stoodUp"])
    for rank, name in enumerate(["gift.insulted", "gift.disliked", "gift", "gift.liked", "gift.loved"], start=1):
        add(name, GIFT_ANSWER, GIFT_F, rank)
    for name, code in MOMENTS.items():
        required = True
        if name == "crime.approve":
            required = alignment < 0
        elif name == "crime.disapprove":
            required = alignment > 0
        add(name, SAY, MOMENT_F, code, required)
    # a murder she saw (optional: without them her crime line plays)
    add("crime.approve.murder", SAY, MOMENT_F, 33, required=False)
    add("crime.disapprove.murder", SAY, MOMENT_F, 34, required=False)
    # two partners have it out (optional: the scene plays only when both add-ons have their lines). In either role:
    # A (heard or saw it) asks, B claims you, A answers, B asks you "Is this true?" and answers yours, A's last word.
    # Claims and answers go by the speaker's own stage. The "expose.reply.*" lines (hers to your answer, in the talk)
    # take the plain outcome's words when an add-on gives none.
    def moment_row(code):
        return condition("GetFactionRankConditionData", "Faction", fw(MOMENT_F), code, "EqualTo")

    def stage_row(stage):
        return condition("GetFactionRankConditionData", "Faction", fw(STAGE_F), stage, "EqualTo")

    add("expose.ask", SAY, MOMENT_F, EXPOSE_ASK, required=False)
    for stage, name in enumerate(["dating", "committed", "engaged", "spouse"], start=2):
        add(f"expose.claim.{name}", SAY, MOMENT_F, EXPOSE_CLAIM, required=False, extra=[stage_row(stage)])
        add(f"expose.counter.{name}", SAY, MOMENT_F, EXPOSE_COUNTER, required=False, extra=[stage_row(stage)])
    add("expose.question", FORCEGREET, MOMENT_F, EXPOSE_QUESTION, required=False)
    add("expose.reply.disowned", EXPOSE_SIDE_A, MOMENT_F, EXPOSE_QUESTION, required=False, goodbye=True)
    add("expose.reply.believed", EXPOSE_SIDE_B, EXPOSE_F, 1, required=False, goodbye=True, extra=[moment_row(EXPOSE_QUESTION)])
    add("expose.reply.disbelieved", EXPOSE_SIDE_B, EXPOSE_F, 2, required=False, goodbye=True, extra=[moment_row(EXPOSE_QUESTION)])
    add("expose.reply.true", EXPOSE_TRUE, MOMENT_F, EXPOSE_QUESTION, required=False, goodbye=True)
    for rank, name in enumerate(EXPOSE_OUTCOMES, start=1):
        add(f"expose.{name}", SAY, EXPOSE_F, rank, required=False, extra=[moment_row(EXPOSE_OUTCOME)])
    for name, code in EVENTS.items():
        add(f"reaction.{name}", REACTION_HELLO, REACTION_F, code, required=False)
        add(f"recent.{name}", RECENT_HELLO, RECENT_F, code, required=False)
    return r


def fw(local):
    return f"{local:06X}:{FRAMEWORK}"


def condition(function, field, value, compare, op="EqualTo", or_next=False):
    fields = {"CompareOperator": op, "ComparisonValue": str(compare)}
    if or_next:
        fields["Flags"] = "OR"  # ORed with the row after it (OR binds before AND)
    return {"type": "ConditionFloat", "fields": fields,
            "sets": [{"path": "Data", "compose": {"type": function, "fields": {"RunOnType": "Subject"},
                                                  "sets": [{"path": field, "value": value}]}}]}


def random_row(percent):
    """GetRandomPercent < percent: the line plays only part of the time"""
    return {"type": "ConditionFloat", "fields": {"CompareOperator": "LessThan", "ComparisonValue": str(percent)},
            "sets": [{"path": "Data", "compose": {"type": "GetRandomPercentConditionData"}}]}


def share_rows(percent):
    """(IRF_StageFaction != Married OR GetRandomPercent < percent): only a spouse's lines are thinned out"""
    return [condition("GetFactionRankConditionData", "Faction", fw(STAGE_F), MARRIED, "NotEqualTo", or_next=True),
            {"type": "ConditionFloat", "fields": {"CompareOperator": "LessThan", "ComparisonValue": str(percent)},
             "sets": [{"path": "Data", "compose": {"type": "GetRandomPercentConditionData"}}]}]


def rank_rows(faction, rank, op):
    """one GetFactionRank row, or for a list of ranks an OR group (== each); none without a faction"""
    if faction is None:
        return []
    if not isinstance(rank, list):
        return [condition("GetFactionRankConditionData", "Faction", fw(faction), rank, op)]
    return [condition("GetFactionRankConditionData", "Faction", fw(faction), r, "EqualTo", or_next=i < len(rank) - 1)
            for i, r in enumerate(rank)]


def adopted_conditions(text):
    """{"boys": 0 | ">=1", "girls": ...} -> GetGlobalValue rows on IRF_AdoptedBoys / IRF_AdoptedGirls"""
    rows = []
    for name, local in ADOPTED.items():
        if not isinstance(text, dict) or name not in text:
            continue
        want = str(text[name]).strip()
        op = next((o for o in sorted(OPS, key=len, reverse=True) if want.startswith(o)), "==")
        value = want[len(op):].strip() if want.startswith(op) else want
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": OPS[op], "ComparisonValue": value},
                     "sets": [{"path": "Data", "compose": {"type": "GetGlobalValueConditionData",
                                                           "sets": [{"path": "Global", "value": fw(local)}]}}]})
    return rows


def life_conditions(text):
    """{"alive": ref} / {"dead": ref} -> GetDead on that reference == 0 / == 1"""
    rows = []
    for name, dead in (("alive", 0), ("dead", 1)):
        if isinstance(text, dict) and name in text:
            rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": str(dead)},
                         "sets": [{"path": "Data", "compose": {"type": "GetDeadConditionData", "fields": {"RunOnType": "Reference"},
                                                               "sets": [{"path": "Reference", "value": form_key(text[name])}]}}]})
    return rows


def feeling_conditions(text):
    """{"rank": "<0" | {"toward": ref, "is": "<0"}} -> GetRelationshipRank of the speaker; {"stageDone": "Q|0x..:n"}"""
    rows = []
    if not isinstance(text, dict):
        return rows
    if "rank" in text:
        want = text["rank"] if isinstance(text["rank"], dict) else {"toward": "Skyrim.esm|0x000014", "is": text["rank"]}
        compare = str(want["is"]).strip()
        op = next(o for o in sorted(OPS, key=len, reverse=True) if compare.startswith(o))
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": OPS[op], "ComparisonValue": compare[len(op):].strip()},
                     "sets": [{"path": "Data", "compose": {"type": "GetRelationshipRankConditionData", "fields": {"RunOnType": "Subject"},
                                                           "sets": [{"path": "TargetNpc", "value": form_key(want["toward"])}]}}]})
    if "stageDone" in text:
        quest, stage = text["stageDone"].rsplit(":", 1)
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": "1"},
                     "sets": [{"path": "Data", "compose": {"type": "GetStageDoneConditionData", "fields": {"RunOnType": "Subject", "Stage": stage},
                                                           "sets": [{"path": "Quest", "value": form_key(quest)}]}}]})
    return rows


def kids_conditions(text):
    """{"kids": ">=1"} -> IRF_AdoptedBoys >= 1 OR IRF_AdoptedGirls >= 1; {"kids": 0} -> both == 0"""
    if not isinstance(text, dict) or "kids" not in text:
        return []
    some = str(text["kids"]).strip() != "0"
    return [{"type": "ConditionFloat", "fields": {"CompareOperator": "GreaterThanOrEqualTo" if some else "EqualTo",
                                                  "ComparisonValue": "1" if some else "0", **({"Flags": "OR"} if some and i == 0 else {})},
             "sets": [{"path": "Data", "compose": {"type": "GetGlobalValueConditionData",
                                                   "sets": [{"path": "Global", "value": fw(local)}]}}]}
            for i, local in enumerate(ADOPTED.values())]


def stage_rows_on(ref, ranks):
    """GetFactionRank IRF_StageFaction == each rank, run on the candidate (an OR group): a circle member's greeting"""
    return [{"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": str(r), **({"Flags": "OR"} if i < len(ranks) - 1 else {})},
             "sets": [{"path": "Data", "compose": {"type": "GetFactionRankConditionData", "fields": {"RunOnType": "Reference"},
                                                   "sets": [{"path": "Faction", "value": fw(STAGE_F)}, {"path": "Reference", "value": ref}]}}]}
            for i, r in enumerate(ranks)]


def place_conditions(text):
    """{"following": bool} -> GetPlayerTeammate on her; {"near": {...}} -> GetDistance (+ GetLineOfSight) to them;
    {"set": name} -> the speaker's own IRF_StageFaction rank (an OR group); {"player": "male"} -> GetPCIsSex"""
    rows = []
    if not isinstance(text, dict):
        return rows
    if "following" in text:
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": "1" if text["following"] else "0"},
                     "sets": [{"path": "Data", "compose": {"type": "GetPlayerTeammateConditionData", "fields": {"RunOnType": "Reference"},
                                                           "sets": [{"path": "Reference", "value": HER[0]}]}}]})
    if "near" in text:
        near = text["near"]
        who = form_key(near["who"]) if near.get("who") else HER[0]
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "LessThan", "ComparisonValue": str(near.get("within", 700))},
                     "sets": [{"path": "Data", "compose": {"type": "GetDistanceConditionData", "fields": {"RunOnType": "Subject"},
                                                           "sets": [{"path": "Target", "value": who}]}}]})
        if near.get("sees", False):
            rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": "1"},
                         "sets": [{"path": "Data", "compose": {"type": "GetLineOfSightConditionData", "fields": {"RunOnType": "Subject"},
                                                               "sets": [{"path": "Target", "value": who}]}}]})
    if "set" in text:
        rows += rank_rows(STAGE_F, SETS[text["set"]], "EqualTo")
    if text.get("jealousOf", "both") != "both":
        kinds = {"fighter": 1, "nonfighter": 2}
        if text["jealousOf"] not in kinds:
            sys.exit(f'"jealousOf" is "fighter", "nonfighter" or "both", not {text["jealousOf"]!r}')
        rows.append(condition("GetFactionRankConditionData", "Faction", fw(JEALOUS_OF_F), kinds[text["jealousOf"]], "EqualTo"))
    if "player" in text:
        rows.append({"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": "1"},
                     "sets": [{"path": "Data", "compose": {"type": "GetPCIsSexConditionData",
                                                           "fields": {"RunOnType": "Subject", "MaleFemaleGender": text["player"].capitalize()}}}]})
    if "happy" in text:
        def her_rank(faction, value, op, or_next=False):
            return {"type": "ConditionFloat", "fields": {"CompareOperator": op, "ComparisonValue": str(value), **({"Flags": "OR"} if or_next else {})},
                    "sets": [{"path": "Data", "compose": {"type": "GetFactionRankConditionData", "fields": {"RunOnType": "Reference"},
                                                          "sets": [{"path": "Faction", "value": fw(faction)}, {"path": "Reference", "value": HER[0]}]}}]}
        if text["happy"]:
            rows += [her_rank(BAND_F, HAPPY_BAND, "GreaterThanOrEqualTo"), her_rank(MOOD_F, 0, "EqualTo")]
        else:
            rows += [her_rank(BAND_F, HAPPY_BAND, "LessThan", or_next=True), her_rank(MOOD_F, 0, "GreaterThan")]
    return rows


def specific(text):
    """a boys/girls text: first-fit, before the random pick"""
    return isinstance(text, dict) and any(name in text for name in ADOPTED)


def info(editor_id, topic, text, speaker_npc, faction, rank, op, goodbye, random=None, rows=None):
    extra = (rows or []) + adopted_conditions(text) + life_conditions(text) + feeling_conditions(text) + kids_conditions(text) + place_conditions(text)
    raw = text
    prompt = text.get("prompt") if isinstance(text, dict) else None
    text = text["text"] if isinstance(text, dict) else text
    parts = text if isinstance(text, list) else [text]  # several responses: said one after another
    ops = [{"field_path": "Topic", "value": fw(topic)}] + [
        {"field_path": "Responses", "op": "Add", "compose": {"type": "DialogResponse", "fields": {
            "Emotion": "Neutral", "EmotionValue": "50", "ResponseNumber": str(n), "Text": part}}}
        for n, part in enumerate(parts, start=1)] + [
        {"field_path": "Conditions", "op": "Add", "composes": [
            condition("GetIsIDConditionData", "Object", speaker_npc, 1)] + rank_rows(faction, rank, op) + extra},
    ]
    if prompt:
        ops.append({"field_path": "Prompt", "value": prompt})  # the player's words for this line
    once = isinstance(raw, dict) and raw.get("once") == "day"
    flags = (["Goodbye"] if goodbye else []) + (["Random"] if random else []) + (["RandomEnd"] if random == "end" else []) + (["SayOnce"] if once else [])
    if flags:
        ops.append({"field_path": "Flags.Flags", "value": ", ".join(flags)})
    if once:
        ops.append({"field_path": "Flags.ResetHours", "value": "24"})  # once a day
    return {"record_type": "DialogResponses", "editorid": editor_id, "parent": fw(topic), "ops": ops}


def form_key(spec_form):
    """'Skyrim.esm|0x01347B' -> '01347B:Skyrim.esm' (houseCARL's form)"""
    plugin, local = spec_form.split("|")
    return f"{int(local, 16) & 0xFFFFFF:06X}:{plugin}"


def texts(value):
    return value if isinstance(value, list) else [value]


def spoken(text):
    """the words a line says (what its voice file records): its text, or its responses joined"""
    words = text["text"] if isinstance(text, dict) else text
    return "\n".join(words) if isinstance(words, list) else words


def main(spec_path, out_dir, relock=False, into=None):
    spec = json.loads(Path(spec_path).read_text(encoding="utf-8"))
    plugin = spec["plugin"]
    into = into or plugin  # the file the records are built in (a temporary name, renamed after)
    who = spec["candidate"]
    short = "".join(ch for ch in who["name"].split()[0] if ch.isalnum())
    npc = form_key(who["npc"])
    HER[0] = form_key(who["ref"])
    reqs = requirements(spec.get("attributes", {}))
    records, lines = [], {}  # until the end, lines name records by EditorID
    used = set()
    # her own other records first (a spouse perk): built like any record, named by their EditorID
    for record in spec.get("records", []):
        records.append(record)
        used.add(record["editorid"])
    attributes = json.loads(json.dumps(spec.get("attributes", {})))

    def word_id(base, text):
        """a line's EditorID: what it is + a hash of its words, so the same words keep the same FormID"""
        h = hashlib.sha1(spoken(text).encode("utf-8")).hexdigest()[:6]
        eid, n = f"{base}_{h}", 2
        while eid in used:  # the same words twice for one key
            eid, n = f"{base}_{h}_{n}", n + 1
        used.add(eid)
        return eid

    def emit(key, base, topic, text, speaker, faction, rank, op="EqualTo", goodbye=False, random=None, rows=None):
        record = info(word_id(base, text), topic, text, speaker, faction, rank, op, goodbye, random, rows)
        if key in LINKS:
            for topic in LINKS[key]:
                record["ops"].append({"field_path": "LinkTo", "op": "Add", "value": fw(topic)})
        records.append(record)
        lines.setdefault(key, []).append(record["editorid"])
        add_reply(record["editorid"], text)

    replies = []  # [{"after": [line], "lines": [her answers]}]: the manifest's

    def add_reply(after, text):
        """her answer to the line just made (a "reply" on its text): IRF_SayTopic, moment kReply, IRF_ReplyFaction == n"""
        if not isinstance(text, dict) or "reply" not in text:
            return
        n = len(replies) + 1
        answers = []
        for answer in texts(text["reply"]):
            records.append(info(word_id(f"IRF{short}_reply", answer), SAY, answer, npc,
                                MOMENT_F, REPLY_MOMENT, "EqualTo", False, None,
                                [condition("GetFactionRankConditionData", "Faction", fw(REPLY_F), n, "EqualTo")]))
            answers.append(records[-1]["editorid"])
        replies.append({"after": [after], "lines": answers})

    given = spec.get("lines", {})
    for name in EXPOSE_OUTCOMES:  # her answer in the talk: the plain outcome's words unless the add-on gives its own
        if f"expose.{name}" in given and f"expose.reply.{name}" not in given:
            given[f"expose.reply.{name}"] = given[f"expose.{name}"]
    LINKS["court"] = [KNOW + i - 1 for i in range(1, KNOW_MAX + 1) if f"know.{i}" in given or i == 1]

    def values_of(key, required):
        if key in given:
            return texts(given[key])
        if required and spec.get("placeholders", False):
            return [f"[{who['name']}: {key}]"]
        return []

    # the sets of one topic (hello, goodbye, idle): every line of every set in one random run, each line limited to
    # its set's stages; boys/girls texts join the pick too (they only come up when they fit). A spouse's lines are
    # thinned so that vanilla's spouse chatter still gets about (100 - SPOUSE_IDLE_SHARE) % of her idles.
    set_keys = [k for k, req in reqs.items() if isinstance(req[2], list)]

    def emit_sets(topic):
        run = []
        for k in [k for k in set_keys if reqs[k][0] == topic]:
            vs = values_of(k, reqs[k][5])
            for text in vs:
                run.append((k, f"IRF{short}_{k.replace('.', '_')}", text))
        married = sum(1 for k, _, _ in run if MARRIED in reqs[k][2])
        percent = None
        if topic == IDLE and married:
            percent = round(100 * (1 - (1 - SPOUSE_IDLE_SHARE / 100) ** (1 / married)))
        for i, (k, eid, text) in enumerate(run):
            t, f, r, o, g, _, extra = reqs[k]
            random = None if len(run) < 2 else ("end" if i == len(run) - 1 else "start")
            thin = share_rows(percent) if percent and MARRIED in r else []
            emit(k, eid, t, text, npc, f, r, o, g, random, extra + thin)

    sets_done = set()
    for key, (topic, faction, rank, op, goodbye, required, rows) in reqs.items():
        if key in set_keys:
            if topic not in sets_done:  # after the topic's mood lines, which win when they fit
                sets_done.add(topic)
                emit_sets(topic)
            continue
        values = values_of(key, required)
        if not values:
            continue
        # several texts: one random run (the game picks one of those that fit each time); boys/girls texts stay
        # first-fit, so they go before the run
        values = [v for v in values if specific(v)] + [v for v in values if not specific(v)]
        plain = [i for i, v in enumerate(values) if not specific(v)]
        for i, text in enumerate(values):
            eid = f"IRF{short}_{key.replace('.', '_')}"
            random = None
            if len(plain) > 1 and i in plain:
                random = "end" if i == plain[-1] else "start"
            emit(key, eid, topic, text, npc, faction, rank, op, goodbye, random, rows)

    manifest_circle = []
    for index, member in enumerate(spec.get("circle", [])):
        mnpc = form_key(member["npc"])
        mshort = "".join(ch for ch in member["name"].split()[0] if ch.isalnum())
        entry = {k: member[k] for k in ("name", "npc", "ref")}
        before = dict(lines)
        reactions = {}
        for event, value in member.get("reactions", {}).items():
            variants = texts(value)  # first that fits (no random pick): a rival's or a winner's line before the plain one
            for text in variants:
                key = f"circle.{index}.reaction.{event}"
                emit(key, f"IRF{short}_{mshort}_{event}", REACTION_HELLO, text, mnpc, REACTION_F, EVENTS[event])
                reactions.setdefault(event, []).append(lines[key][-1])
        if reactions:
            entry["reactions"] = reactions
        # their everyday hellos and goodbyes while she's at a stage with you: one random run per topic, thinned so
        # one of them plays greetShare % of the time and their vanilla ones the rest
        greetings = {}
        share = member.get("greetShare", CIRCLE_GREET_SHARE)
        her = form_key(who["ref"])
        for topic, prefix in ((CIRCLE_HELLO, "hello."), (CIRCLE_GOODBYE, "goodbye.")):
            run = [(name, text) for name, value in member.get("greetings", {}).items() if name.startswith(prefix) for text in texts(value)]
            for i, (name, text) in enumerate(run):
                # the lines of one set that can fit at once (a rival's and a friend's line never do)
                mood = lambda t: json.dumps([t.get(k) for k in ("rank", "stageDone")] if isinstance(t, dict) else None)
                together = sum(1 for other, t in run if other == name and mood(t) == mood(text))
                percent = round(100 * (1 - (1 - share / 100) ** (1 / together)))
                if isinstance(text, dict) and "reply" in text:
                    percent = EXCHANGE_SHARE  # she has to be there and it's once a day: don't make it rarer still
                ranks = CIRCLE_SETS[name[len(prefix):]]
                booked = [condition("GetQuestRunningConditionData", "Quest", WEDDING, 0)] if name.endswith(".maramal") else []
                key = f"circle.{index}.{name}"
                eid = word_id(f"IRF{short}_{mshort}_{name.replace('.', '_')}", text)
                random = None if len(run) < 2 else ("end" if i == len(run) - 1 else "start")
                record = info(eid, topic, text, mnpc, None, None, None, False, random,
                              stage_rows_on(her, ranks) + booked + [random_row(percent)])
                records.append(record)
                lines.setdefault(key, []).append(eid)
                greetings.setdefault(name, []).append(eid)
                add_reply(lines[key][-1], text)
        if greetings:
            entry["greetings"] = greetings
        if "refuse" in member:
            refuse = dict(member["refuse"])
            refuse_lines = []
            for text in texts(refuse.pop("lines")):
                key = f"circle.{index}.refuse"
                emit(key, f"IRF{short}_{mshort}_refuse", SAY, text, mnpc, MOMENT_F, REFUSE_MOMENT)
                refuse_lines.append(lines[key][-1])
            refuse["lines"] = refuse_lines
            entry["refuse"] = refuse
        manifest_circle.append(entry)
        for key in list(lines):
            if key.startswith("circle.") and key not in before:
                del lines[key]  # circle lines live under the circle entry, not in "lines"

    # the children's greetings when something is wrong at home: said by whichever child it is (no speaker check), while
    # you're married to her (her stage read off her), voiced in both child voices
    children = {}
    for state, said in spec.get("children", {}).items():
        if state not in CHILD_STATES:
            sys.exit(f'children: "{state}" is one of {list(CHILD_STATES)}')
        for text in (said if isinstance(said, list) else [said]):
            married = {"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": str(MARRIED)},
                       "sets": [{"path": "Data", "compose": {"type": "GetFactionRankConditionData", "fields": {"RunOnType": "Reference"},
                                                             "sets": [{"path": "Faction", "value": fw(STAGE_F)}, {"path": "Reference", "value": HER[0]}]}}]}
            record = info(word_id(f"IRF{short}_children_{state}", text), CHILD_HELLO, text, npc, CHILD_F, CHILD_STATES[state],
                          "EqualTo", False, None, [married])
            for op in record["ops"]:
                if op.get("field_path") == "Conditions":
                    op["composes"] = op["composes"][1:]  # whichever child: no GetIsID
            records.append(record)
            children.setdefault(state, []).append(record["editorid"])

    # her family conversations: both halves are hers to write; the child's lines are said by whichever child it is
    # (no speaker check; the conversation's number is read off her), voiced in both child voices
    family = []
    for n, talk in enumerate(spec.get("family", []), start=1):
        when = talk.get("when", {})
        homes = when.get("home", "fine")
        homes = homes if isinstance(homes, list) else [homes]
        if not homes or any(h not in FAMILY_HOMES for h in homes):
            sys.exit(f'family {n}: "home" is one or more of {FAMILY_HOMES}')
        entry = {"home": homes, "lines": []}
        for key in ("mood", "alive", "dead"):  # her mood; only while someone lives / after they died (a placed actor)
            if when.get(key):
                entry[key] = when[key]
        if talk.get("child") in ("boy", "girl"):
            entry["child"] = talk["child"]
        for k, line in enumerate(talk["lines"], start=1):
            by_child = "child" in line
            said = line["child"] if by_child else line["her"]
            versions = said if isinstance(said, list) else [said]  # versions: [{"text", "player": "male"}, ...]
            step = condition("GetFactionRankConditionData", "Faction", fw(FAMILY_STEP_F), k, "EqualTo")
            if by_child:
                which = {"type": "ConditionFloat", "fields": {"CompareOperator": "EqualTo", "ComparisonValue": str(n)},
                         "sets": [{"path": "Data", "compose": {"type": "GetFactionRankConditionData", "fields": {"RunOnType": "Reference"},
                                                               "sets": [{"path": "Faction", "value": fw(FAMILY_F)}, {"path": "Reference", "value": HER[0]}]}}]}
            else:
                which = condition("GetFactionRankConditionData", "Faction", fw(FAMILY_F), n, "EqualTo")
            made = []
            for text in versions:
                record = info(word_id(f"IRF{short}_family_{n}_{k}{'_child' if by_child else ''}", text), SAY, text, npc,
                              MOMENT_F, FAMILY_MOMENT, "EqualTo", False, None, [step, which])
                if by_child:  # whichever child it is: no GetIsID
                    for op in record["ops"]:
                        if op.get("field_path") == "Conditions":
                            op["composes"] = op["composes"][1:]
                records.append(record)
                made.append(record["editorid"])
            spoken_line = {"who": "child" if by_child else "her", "line": made[0]}
            if len(made) > 1:
                spoken_line["versions"] = made[1:]  # the same step, another version (the game plays the one that fits)
            entry["lines"].append(spoken_line)
        if len(entry["lines"]) < 2:
            sys.exit(f"family {n}: a conversation needs two lines at least")
        family.append(entry)

    # FormIDs: the lock's for every record it knows (voice files are named by them), the next free ones for the rest
    lock_path = Path(spec_path).parent / spec["lock"] if spec.get("lock") else None
    lock = {} if relock or not lock_path or not lock_path.exists() else \
        {e: int(v, 16) for e, v in json.loads(lock_path.read_text(encoding="utf-8")).items()}
    ids, nxt = {}, max(lock.values(), default=0x7FF) + 1
    for record in records:
        eid = record["editorid"]
        if eid in lock:
            ids[eid] = lock[eid]
        else:
            ids[eid], nxt = nxt, nxt + 1
    retired = {e: i for e, i in lock.items() if e not in ids}  # removed or reworded: their FormID stays taken
    taken = sorted(list(ids.values()) + list(retired.values()))
    if taken != list(range(0x800, 0x800 + len(taken))):
        sys.exit(f"STOP: the FormIDs aren't 0x800..0x{0x800 + len(taken) - 1:03X} without gaps; the lock is damaged")

    # each line's place in its topic: PNAM = the line before it there. The game reads the plugin in FormID order
    # and slots a line after its PNAM when that one is already read, else adds it at the end: check that this
    # gives the wanted order before anything is built
    order_ops, problems = [], []
    topics = {}
    for record in records:
        if record.get("record_type") == "DialogResponses":
            topics.setdefault(record["parent"], []).append(record["editorid"])
    for topic, wanted in topics.items():
        prev = {e: (wanted[i - 1] if i else None) for i, e in enumerate(wanted)}
        placed = []
        for eid in sorted(wanted, key=lambda e: ids[e]):
            p = prev[eid]
            placed.insert(placed.index(p) + 1 if p in placed else len(placed), eid)
        if placed != wanted:
            problems.append(f"{topic}: the game would read its lines in another order than the spec's "
                            f"(a new line can't go first in a topic that already has older lines)")
        for eid in wanted:
            if prev[eid]:
                order_ops.append({"formid": f"{ids[eid]:06X}:{into}", "field_path": "PreviousDialog",
                                  "value": f"{ids[prev[eid]]:06X}:{into}"})
    if problems:
        print("STOP: some lines wouldn't land where the spec puts them:\n  " + "\n  ".join(problems))
        print("Move the new line below an older one, or pass --relock (only while no voice files exist).")
        sys.exit(2)

    # build order: by FormID, a placeholder global holding each retired FormID
    by_id = {ids[r["editorid"]]: r for r in records}
    for eid, i in retired.items():
        by_id[i] = {"record_type": "GlobalShort", "editorid": f"{eid}_retired",
                    "ops": [{"field_path": "Data", "value": "0"}]}  # a global needs its value (FLTV) to read back
    build = [by_id[i] for i in sorted(by_id)]

    def ref(eid):
        return f"{plugin}|0x{ids[eid]:03X}"

    def refs(value):
        if isinstance(value, list):
            return [refs(v) for v in value]
        if isinstance(value, dict):
            return {k: refs(v) for k, v in value.items()}
        return ref(value) if isinstance(value, str) and value in ids else value

    for name, value in attributes.items():
        if isinstance(value, str) and value.startswith("@"):
            attributes[name] = ref(value[1:])
    manifest = {"format": 1, "candidate": who, "attributes": attributes, "lines": refs(lines)}
    if manifest_circle:
        manifest["circle"] = []
        for m in manifest_circle:
            m = {k: (refs(v) if k in ("reactions", "greetings") else v) for k, v in m.items()}
            if "refuse" in m:
                m["refuse"] = {**m["refuse"], "lines": refs(m["refuse"]["lines"])}
            manifest["circle"].append(m)
    if replies:
        manifest["replies"] = refs(replies)
    if family:
        manifest["family"] = refs(family)
    if children:
        manifest["children"] = refs(children)
    if lock_path:
        new_lock = {**lock, **ids}
        lock_path.write_text(json.dumps({e: f"0x{i:03X}" for e, i in sorted(new_lock.items(), key=lambda x: x[1])}, indent=1) + "\n",
                             encoding="utf-8")
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "records.json").write_text(json.dumps(build, indent=1), encoding="utf-8")
    (out / "order.json").write_text(json.dumps(order_ops, indent=1), encoding="utf-8")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    new = [e for e in ids if e not in lock]
    print(f"{len(build)} records ({len(new)} new, {len(retired)} retired placeholders) -> {out / 'records.json'}; "
          f"{len(order_ops)} PNAMs -> {out / 'order.json'}; manifest -> {out / 'manifest.json'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage=__doc__)
    parser.add_argument("spec")
    parser.add_argument("out")
    parser.add_argument("--relock", action="store_true")
    parser.add_argument("--major", action="store_true", help="with --relock on a released add-on: a breaking new major version")
    parser.add_argument("--into")
    a = parser.parse_args()
    if a.relock and json.loads(Path(a.spec).read_text(encoding="utf-8")).get("released") and not a.major:
        sys.exit("STOP: this add-on is released: --relock renumbers its lines and breaks other mods' patches and translations. "
                 "Only for a new major version (a breaking release, new game advised): add --major.")
    main(a.spec, a.out, a.relock, a.into)
