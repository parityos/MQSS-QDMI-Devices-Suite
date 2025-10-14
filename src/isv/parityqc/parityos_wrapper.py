# ParityQC © 2025. See LICENSE.txt in /src/isv/parityqc/ for details.
"""
TODO: Make sure parityos is available whenever this script is executed.

FIXME: docstring
"""

from parityos.services.client import HTTPClient

# FIXME: replace URL and username with variable
client = HTTPClient(username="admin", host="http://localhost:8000/v3")
