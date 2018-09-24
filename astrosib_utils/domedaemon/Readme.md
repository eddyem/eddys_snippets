Baader all-sky dome control network daemon
==================

Open a socket at given port (default: 55555), works with http & direct requests.

**Protocol**

Send requests over socket (by curl or something else) or http requests (in browser).

* *open*    - open dome;
* *close*   - close dome;
* *status*  - dome state (return "opened", "closed" or "intermediate");
* *weather* - weather status (good weather or rain/clouds).
