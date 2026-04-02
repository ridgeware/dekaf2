#!/bin/bash

#%
#% run-smoketests.sh - entrypoint for the dekaf2-suite container
#%
#% Runs unit tests first, then optionally KSQL smoke tests against
#% databases configured via environment variables.
#%
#% environment:
#%   BUILDTYPE        :: "release" or "debug" (default: release)
#%   MYSQL_HOST       :: if set, run smoke tests against this MySQL host
#%   MYSQL_PORT       :: MySQL port (default: 3306)
#%   POSTGRESQL_HOST  :: if set, run smoke tests against this PostgreSQL host
#%   POSTGRESQL_PORT  :: PostgreSQL port (default: 5432)
#%   DB_USER          :: database user     (default: dekaf2)
#%   DB_PASS          :: database password (default: dekaf2test)
#%   DB_NAME          :: database name     (default: dekaf2test)
#%

set -o pipefail

BUILDTYPE="${BUILDTYPE:-release}"
DB_USER="${DB_USER:-dekaf2}"
DB_PASS="${DB_PASS:-dekaf2test}"
DB_NAME="${DB_NAME:-dekaf2test}"

UTESTS_DIR="/home/dekaf2/utests/build/${BUILDTYPE}"
SMOKETESTS_DIR="/home/dekaf2/smoketests/build/${BUILDTYPE}"
CREATEDBC="/usr/local/bin/createdbc"
DBC_DIR="/tmp/dbc"

FAIL=0

#----------------------------------------------------------------------
wait_for_port ()
#----------------------------------------------------------------------
{
	local host="$1" port="$2" maxwait="${3:-60}" elapsed=0

	echo ":: waiting for ${host}:${port} (max ${maxwait}s) ..."

	while [ ${elapsed} -lt ${maxwait} ]; do
		if echo | nc -w 1 "${host}" "${port}" >/dev/null 2>&1; then
			echo ":: ${host}:${port} is ready (${elapsed}s)"
			return 0
		fi
		sleep 2
		elapsed=$((elapsed + 2))
	done

	echo ":: ERROR: ${host}:${port} did not become ready within ${maxwait}s"
	return 1
}

# --- unit tests (no DB needed) ---

echo "=== Running unit tests ==="
cd "${UTESTS_DIR}" || { echo ":: ERROR: utests build dir not found"; exit 1; }
./dekaf2-utests || FAIL=1

# --- DB smoke tests (only if configured) ---

if [ -z "${MYSQL_HOST}" ] && [ -z "${POSTGRESQL_HOST}" ]; then
	echo ":: no database configured - skipping smoke tests"
	exit ${FAIL}
fi

[ -x "${CREATEDBC}" ] || { echo ":: ERROR: createdbc not found at ${CREATEDBC}"; exit 1; }
mkdir -p "${DBC_DIR}"

# MySQL
if [ -n "${MYSQL_HOST}" ]; then
	MYSQL_PORT="${MYSQL_PORT:-3306}"

	wait_for_port "${MYSQL_HOST}" "${MYSQL_PORT}" 60 || { FAIL=1; }

	if [ ${FAIL} -eq 0 ] || true; then
		# give the server a moment to finish post-ready initialization
		sleep 3

		echo "=== Running smoke tests against MySQL (${MYSQL_HOST}:${MYSQL_PORT}) ==="
		${CREATEDBC} "${DBC_DIR}/mysql.dbc" mysql \
			"${DB_USER}" "${DB_PASS}" "${DB_NAME}" \
			"${MYSQL_HOST}" "${MYSQL_PORT}" \
			|| { echo ":: ERROR: failed to create MySQL DBC file"; FAIL=1; }

		if [ -f "${DBC_DIR}/mysql.dbc" ]; then
			cd "${SMOKETESTS_DIR}" || { echo ":: ERROR: smoketests build dir not found"; exit 1; }
			./dekaf2-smoketests "KSQL" -dbc "${DBC_DIR}/mysql.dbc" || FAIL=1
		fi
	fi
fi

# PostgreSQL
if [ -n "${POSTGRESQL_HOST}" ]; then
	POSTGRESQL_PORT="${POSTGRESQL_PORT:-5432}"

	wait_for_port "${POSTGRESQL_HOST}" "${POSTGRESQL_PORT}" 60 || { FAIL=1; }

	if [ ${FAIL} -eq 0 ] || true; then
		sleep 3

		echo "=== Running smoke tests against PostgreSQL (${POSTGRESQL_HOST}:${POSTGRESQL_PORT}) ==="
		${CREATEDBC} "${DBC_DIR}/postgresql.dbc" postgresql \
			"${DB_USER}" "${DB_PASS}" "${DB_NAME}" \
			"${POSTGRESQL_HOST}" "${POSTGRESQL_PORT}" \
			|| { echo ":: ERROR: failed to create PostgreSQL DBC file"; FAIL=1; }

		if [ -f "${DBC_DIR}/postgresql.dbc" ]; then
			cd "${SMOKETESTS_DIR}" || { echo ":: ERROR: smoketests build dir not found"; exit 1; }
			./dekaf2-smoketests "KSQL" -dbc "${DBC_DIR}/postgresql.dbc" || FAIL=1
		fi
	fi
fi

# --- summary ---

if [ ${FAIL} -eq 0 ]; then
	echo "=== All tests PASSED ==="
else
	echo "=== Some tests FAILED ==="
fi

exit ${FAIL}
