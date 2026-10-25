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
  "placeholders": true,                      fill every required key that has no text
  "circle": [ { "name": "Lucan", "npc": "...", "ref": "...",
                "reactions": { "amends": "Text" },
                "refuse": { "on": ["brokenHeart"], "until": "amends", "services": ["trade"], "lines": "Text" } } ]
}

Usage: python irf_addon.py spec.json out_folder
"""

import json
import sys
from pathlib import Path

FRAMEWORK = "Immersive Romance Framework.esp"

# topics (Immersive Romance Framework.esp)
HELLO, GOODBYE, COURT, HOWAREWE, AMULET = 0x806, 0x807, 0x808, 0x80A, 0x80D
REACTION_HELLO, ABSENCE_HELLO, RECENT_HELLO, SAY = 0x823, 0x825, 0x827, 0x829
GIFT_ANSWER = 0x846  # her goodbye after a gift
STAYHOME = 0x848  # the force greet at the town's edge (spouse with children)
ASKDATE, FOLLOW, KISS, BREAKUP, AMENDS, WINBACK, SECRET, TALK, DENY, ADMIT = (
    0x82A, 0x82C, 0x82E, 0x830, 0x832, 0x834, 0x836, 0x838, 0x83A, 0x83C)
# factions
STAGE_F, GREET_F, AMULET_F, BAND_F = 0x801, 0x803, 0x80C, 0x80F
ABSENCE_F, REACTION_F, RECENT_F, MOMENT_F = 0x812, 0x813, 0x814, 0x816
ASKDATE_F, FOLLOW_F, KISS_F, BREAKUP_F, AMENDS_F, WINBACK_F, SECRET_F, TALK_F, CONFRONT_F, TAKEN_F = (
    0x818, 0x819, 0x81A, 0x81B, 0x81C, 0x81D, 0x81E, 0x81F, 0x820, 0x821)
GIFT_F = 0x841  # her answer to a gift: 1 insulted, 2 disliked, 3 plain, 4 liked, 5 loved
MOOD_F, IDLE = 0x85B, 0x85C  # her mood (IRF_MoodFaction), her idles (IRF_IdleTopic, by IRF_StageFaction)
MOODS = {"strained": 2, "jealous": 3, "hurt": 4, "wary": 5}
SPOUSE_IDLE_SHARE = 50  # a spouse's plain idles play this % of the time: vanilla's spouse chatter mixes in
ADOPTED = {"boys": 0x859, "girls": 0x85A}  # IRF_AdoptedBoys / IRF_AdoptedGirls (globals)
OPS = {"==": "EqualTo", "!=": "NotEqualTo", ">=": "GreaterThanOrEqualTo", "<=": "LessThanOrEqualTo", ">": "GreaterThan", "<": "LessThan"}

EVENTS = {"courting": 1, "dating": 2, "committed": 3, "engaged": 4, "married": 5, "amends": 6,
          "playerEnded": 7, "theyEnded": 8, "brokenHeart": 9, "seeingSomeone": 10, "widowed": 11, "wonBack": 12, "killed": 13}
WHYS = {"cheating": 1, "violence": 2, "absence": 3, "bleedouts": 4, "crime": 5, "choice": 6}
REFUSE_MOMENT = 7  # a circle member refusing service
STAYHOME_MOMENT = 8  # a spouse with children won't leave town
MOMENTS = {"caught": 1, "revive": 2, "attacked": 3, "crime.approve": 4, "crime.disapprove": 5, "jealous": 6,
           "road.afterfight": 10, "road.dragon": 11, "road.nearlydead": 12, "road.rest": 13,
           "road.nordicruin": 20, "road.dwarvenruin": 21, "road.cave": 22, "road.dungeon": 23,
           "road.town": 24, "road.inn": 25, "road.hometown": 26}


def requirements(alignment):
    """key -> (topic, faction, rank, operator, goodbye flag, required, extra condition rows); mirrors Candidates.cpp.
    A mood version comes before its plain line: the game plays the first line that fits."""
    r = {}

    def add(key, topic, faction, rank, required=True, op="EqualTo", goodbye=False, extra=None):
        r[key] = (topic, faction, rank, op, goodbye, required, extra or [])

    def mood_row(mood):
        return condition("GetFactionRankConditionData", "Faction", fw(MOOD_F), MOODS[mood], "EqualTo")

    add("court", COURT, TAKEN_F, 1)
    add("court.taken", COURT, TAKEN_F, 2)
    for rank, name in enumerate(["courting", "dating", "committed", "engaged", "spouse"], start=1):
        for mood in MOODS:
            if mood != "hurt":  # the broken heart has its own greetings
                add(f"hello.{name}.{mood}", HELLO, GREET_F, rank, required=False, extra=[mood_row(mood)])
                add(f"goodbye.{name}.{mood}", GOODBYE, GREET_F, rank, required=False, extra=[mood_row(mood)])
        add(f"hello.{name}", HELLO, GREET_F, rank)
        add(f"goodbye.{name}", GOODBYE, GREET_F, rank)
        # her idles, by stage (IRF_StageFaction 1-5): the mood versions first
        for mood in MOODS:
            add(f"idle.{name}.{mood}", IDLE, STAGE_F, rank, required=False, extra=[mood_row(mood)])
        share = [] if name != "spouse" else [{"type": "ConditionFloat", "fields": {"CompareOperator": "LessThan",
                 "ComparisonValue": str(SPOUSE_IDLE_SHARE)}, "sets": [{"path": "Data", "compose": {
                 "type": "GetRandomPercentConditionData"}}]}]
        add(f"idle.{name}", IDLE, STAGE_F, rank, extra=share)
    add("idle.ex", IDLE, STAGE_F, 6)
    add("hello.brokenheart.silent", HELLO, GREET_F, 10, goodbye=True)
    add("hello.brokenheart.hurt", HELLO, GREET_F, 11)
    add("goodbye.brokenheart", GOODBYE, GREET_F, 11)
    for name, why in WHYS.items():
        add(f"hello.ex.{name}", HELLO, GREET_F, 20 + why)
        add(f"goodbye.ex.{name}", GOODBYE, GREET_F, 20 + why)
        if name != "choice":
            add(f"breakup.{name}", REACTION_HELLO, REACTION_F, 20 + why)
    add("amulet.friend", AMULET, AMULET_F, 1)
    add("amulet.courting", AMULET, AMULET_F, 2)
    add("amulet.dating", AMULET, AMULET_F, 3)
    for band in range(1, 6):
        add(f"howarewe.{band}", HOWAREWE, BAND_F, band)
    add("askdate.yes", ASKDATE, ASKDATE_F, 1)
    add("askdate.notyet", ASKDATE, ASKDATE_F, 2)
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
    add("talk", TALK, TALK_F, 1)
    add("confront", REACTION_HELLO, REACTION_F, 30)
    add("deny.works", DENY, CONFRONT_F, 1)
    add("deny.fails", DENY, CONFRONT_F, 2)
    add("admit", ADMIT, CONFRONT_F, 0, op="GreaterThan")
    for band in range(1, 4):
        add(f"absence.{band}", ABSENCE_HELLO, ABSENCE_F, band)
    add("stayhome", STAYHOME, MOMENT_F, STAYHOME_MOMENT, goodbye=True)
    for rank, name in enumerate(["gift.insulted", "gift.disliked", "gift", "gift.liked", "gift.loved"], start=1):
        add(name, GIFT_ANSWER, GIFT_F, rank)
    for name, code in MOMENTS.items():
        required = True
        if name == "crime.approve":
            required = alignment < 0
        elif name == "crime.disapprove":
            required = alignment > 0
        add(name, SAY, MOMENT_F, code, required)
    for name, code in EVENTS.items():
        add(f"reaction.{name}", REACTION_HELLO, REACTION_F, code, required=False)
        add(f"recent.{name}", RECENT_HELLO, RECENT_F, code, required=False)
    return r


def fw(local):
    return f"{local:06X}:{FRAMEWORK}"


def condition(function, field, value, compare, op="EqualTo"):
    return {"type": "ConditionFloat", "fields": {"CompareOperator": op, "ComparisonValue": str(compare)},
            "sets": [{"path": "Data", "compose": {"type": function, "fields": {"RunOnType": "Subject"},
                                                  "sets": [{"path": field, "value": value}]}}]}


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


def info(editor_id, topic, text, speaker_npc, faction, rank, op, goodbye, random=None, rows=None):
    extra = (rows or []) + adopted_conditions(text)
    text = text["text"] if isinstance(text, dict) else text
    ops = [
        {"field_path": "Topic", "value": fw(topic)},
        {"field_path": "Responses", "op": "Add", "compose": {"type": "DialogResponse", "fields": {
            "Emotion": "Neutral", "EmotionValue": "50", "ResponseNumber": "1", "Text": text}}},
        {"field_path": "Conditions", "op": "Add", "composes": [
            condition("GetIsIDConditionData", "Object", speaker_npc, 1),
            condition("GetFactionRankConditionData", "Faction", fw(faction), rank, op)] + extra},
    ]
    flags = (["Goodbye"] if goodbye else []) + (["Random"] if random else []) + (["RandomEnd"] if random == "end" else [])
    if flags:
        ops.append({"field_path": "Flags.Flags", "value": ", ".join(flags)})
    return {"record_type": "DialogResponses", "editorid": editor_id, "parent": fw(topic), "ops": ops}


def form_key(spec_form):
    """'Skyrim.esm|0x01347B' -> '01347B:Skyrim.esm' (houseCARL's form)"""
    plugin, local = spec_form.split("|")
    return f"{int(local, 16) & 0xFFFFFF:06X}:{plugin}"


def texts(value):
    return value if isinstance(value, list) else [value]


def main(spec_path, out_dir):
    spec = json.loads(Path(spec_path).read_text(encoding="utf-8"))
    plugin = spec["plugin"]
    who = spec["candidate"]
    short = "".join(ch for ch in who["name"].split()[0] if ch.isalnum())
    npc = form_key(who["npc"])
    reqs = requirements(spec.get("attributes", {}).get("alignment", 0))
    records, lines = [], {}
    next_id = 0x800

    def emit(key, editor_id, topic, text, speaker, faction, rank, op="EqualTo", goodbye=False, random=None, rows=None):
        nonlocal next_id
        records.append(info(editor_id, topic, text, speaker, faction, rank, op, goodbye, random, rows))
        lines.setdefault(key, []).append(f"{plugin}|0x{next_id:03X}")
        next_id += 1

    given = spec.get("lines", {})
    for key, (topic, faction, rank, op, goodbye, required, rows) in reqs.items():
        if key in given:
            values = texts(given[key])
        elif required and spec.get("placeholders", False):
            values = [f"[{who['name']}: {key}]"]
        else:
            continue
        # several plain texts: one random run (the game picks one each time); texts with conditions (boys,
        # girls) stay first-fit, so they go before the run
        values = [v for v in values if isinstance(v, dict)] + [v for v in values if not isinstance(v, dict)]
        plain = [i for i, v in enumerate(values) if not isinstance(v, dict)]
        for i, text in enumerate(values):
            eid = f"IRF{short}_{key.replace('.', '_')}" + (f"_{i + 1}" if len(values) > 1 else "")
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
            for text in texts(value):
                key = f"circle.{index}.reaction.{event}"
                emit(key, f"IRF{short}_{mshort}_{event}", REACTION_HELLO, text, mnpc, REACTION_F, EVENTS[event])
                reactions.setdefault(event, []).append(lines[key][-1])
        if reactions:
            entry["reactions"] = reactions
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

    manifest = {"format": 1, "candidate": who, "attributes": spec.get("attributes", {}), "lines": lines}
    if manifest_circle:
        manifest["circle"] = manifest_circle
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "records.json").write_text(json.dumps(records, indent=1), encoding="utf-8")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{len(records)} lines -> {out / 'records.json'}; manifest -> {out / 'manifest.json'}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
