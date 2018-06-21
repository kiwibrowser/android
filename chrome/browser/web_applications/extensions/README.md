For historic reasons, Chrome uses the Extension system, more specifically
"Bookmark Apps", to create and install apps based on websites. Hosted Apps are
getting deprecated, so we will eventually migrate off extensions/ and into
web_applications/, but in the meantime we still need to add new features to
the current implementation.

This directory holds files that implement new features for Hosted Apps that will
soon be moved to web_applications/. This is to avoid adding files that will soon
be deleted to extensions/ folders.
