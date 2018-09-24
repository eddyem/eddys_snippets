Astrosib telescope control network daemon
==================

Open a socket at given port (default: 4444), works with http & direct requests.

**Protocol**

Send requests over socket (by curl or something else) or http requests (in browser).

* *open*   - open telescope shutters;
* *close*  - close shutters;
* *status* - shutters' state (return "open", "closed" or "intermediate").
