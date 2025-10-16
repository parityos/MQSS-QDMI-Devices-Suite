# ParityQC © 2025. See LICENSE.txt in /src/isv/parityqc/ for details.
"""
TODO: Make sure parityos is available whenever this script is executed.

FIXME: docstring
"""

print("FIXME: parityos_wrapper start module")

def doit():
    from parityos.services.client import HTTPClient
    # FIXME: replace URL and username with variable
    client = HTTPClient(username="testuser", host="http://localhost:8000/v3")

    print("DONE")

# doit()
print("FIXME: parityos_wrapper end module")