def _sanitize_name(name: str) -> str:
    """Creates a C-compatible identifier (uppercase, underscores instead of dots/dashes)."""
    return name.replace('.', '_').replace('-', '_').upper()