# ParityQC © 2025. See LICENSE.txt in /src/isv/parityqc/ for details.
"""
TODO: Make sure parityos is available whenever this script is executed.

FIXME: docstring
"""

from typing import Optional

print("FIXME: parityos_wrapper start module")

# FIXME: docstring
def create_parityos_client(username: str, base_url: str) -> Optional["HTTPClient"]:
    try:
        print("FIXME: start creating client")
        from parityos.services.client import HTTPClient
        print("FIXME: import HTTPClient")
        client = HTTPClient(username=username, host=base_url)
        print("FIXME: done creating client")
        return client
    except:
        print("FIXME: error! return None.")
        return None

print("FIXME: parityos_wrapper end module")