# ParityQC © 2025. See LICENSE.txt in /src/isv/parityqc/ for details.
"""
TODO: Make sure parityos is available whenever this script is executed.

FIXME: docstring
"""

from typing import Optional

# TODO: this is just toy code for the fake_backend. The actual parityos backend does not need this.
submission_results: dict[int, str] = dict()


# FIXME: docstring
def create_parityos_client(username: str, base_url: str) -> Optional["HTTPClient"]:
    try:
        print("FIXME: start creating client")
        from parityos.services.client import HTTPClient
        client = HTTPClient(username=username, host=base_url)
        print("FIXME: done creating client")
        return client
    except:
        print("FIXME: error! return None.")
        return None


# FIXME: docstring
def submit_job(client: "HTTPClient", program: str) -> int | None:
    # TODO: this is still a fake implementation. The real one would use the client.
    try:
        assert client is not None, "missing client"
        import json
        parsed = json.loads(program)
        assert "content" in parsed
        content = parsed["content"]

        submission_id = _next_submission_id()
        _fake_backend(content, submission_id)
        # TODO: actual remote call will probably take some time but be non-blocking. Caller has to
        # actively poll for the result (`get_result`).

        return submission_id
    except:
        print("FIXME: error in job submission")
        return None

# FIXME: docstring
def get_result(submission_id: int) -> str | None:
    try:
        global submission_results
        if submission_id in submission_results:
            return submission_results[submission_id]
        else:
            return None
    except:
        return None


_gen = None

def _next_submission_id():
    global _gen

    def make_gen():
        i = 0
        while True:
            yield i
            i += 1

    if _gen is None:
        _gen = make_gen()

    return next(_gen)


def _fake_backend(content: str, submission_id) -> None:
    try:
        global submission_results
        result = None

        if content == "hello":
            result = "is english"
        elif content == "hallo":
            result = "is german"
        else:
            result = "invalid input"

        submission_results[submission_id] = result
    except:
        return # fatal error
