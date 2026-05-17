
def normalize_lang(value, default="en"):
    if not isinstance(value, str) or not value.strip():
        return default
    lower = value.strip().lower()
    if lower.startswith("zh"):
        return "zh"
    if lower.startswith("ja"):
        return "ja"
    if lower.startswith("en"):
        return "en"
    return default


def detect_lang(default="en", context=None, *, state, os_module, locale_module, normalize_lang_fn):
    if isinstance(context, dict):
        locale_value = context.get("locale")
        language_value = context.get("language")
        if isinstance(locale_value, str) and locale_value.strip():
            return normalize_lang_fn(locale_value, default)
        if isinstance(language_value, str) and language_value.strip():
            return normalize_lang_fn(language_value, default)

    state_lang = str(state.get("lang", "") or "").strip()
    if state_lang:
        return normalize_lang_fn(state_lang, default)

    locale_env = os_module.environ.get("MALODY_LOCALE", "")
    language_env = os_module.environ.get("MALODY_LANGUAGE", "")
    if locale_env:
        return normalize_lang_fn(locale_env, default)
    if language_env:
        return normalize_lang_fn(language_env, default)

    sys_locale = locale_module.getlocale()[0]
    if sys_locale:
        return normalize_lang_fn(sys_locale, default)
    return default


def tr(context, key, *, translations, detect_lang_fn, **kwargs):
    lang = detect_lang_fn(context=context)
    table = translations.get(key, {})
    text = table.get(lang, table.get("en", key))
    return text.format(**kwargs)
