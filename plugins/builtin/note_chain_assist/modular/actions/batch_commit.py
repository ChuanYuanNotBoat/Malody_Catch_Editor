
def build_batch_edit(payload, callbacks):
    ensure_project_context = callbacks["ensure_project_context"]
    seed_missing_segment_denominators = callbacks["seed_missing_segment_denominators"]
    build_batch_from_curve = callbacks["build_batch_from_curve"]
    context_menu_target_links = callbacks["context_menu_target_links"]

    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    ensure_project_context(context)
    seed_missing_segment_denominators(context)

    if action_id not in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar", "commit_context_segments_to_notes"):
        return {"add": [], "remove": [], "move": []}
    if action_id == "commit_context_segments_to_notes":
        return build_batch_from_curve(context, context_menu_target_links())
    return build_batch_from_curve(context)
