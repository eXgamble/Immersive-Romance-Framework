#pragma once

// What the framework listens to. All event-driven; nothing runs between events.
//   crosshair target change  -> sync the candidate you look at, so the right hello is ready before you talk
//   dialogue menu open       -> sync the speaker (when they started the talk)
//   a line is said           -> actions tied to add-on lines (the court answer starts Courting)
namespace Events
{
	void Register();
}
