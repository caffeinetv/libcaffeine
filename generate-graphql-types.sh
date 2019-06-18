#!/bin/sh
# Introspects and generates client code for the realtime graphql endpoint.
# Assumes caffql, curl, and jq are in your path.
# Set LIBCAFFEINE_DOMAIN to use a environment other than production.

set -e

LIBCAFFEINE_DOMAIN="${LIBCAFFEINE_DOMAIN:-caffeine.tv/}"

echo $LIBCAFFEINE_DOMAIN

CREDENTIAL=$(curl "https://api.${LIBCAFFEINE_DOMAIN}v1/credentials/anonymous" | jq -j .credential)

curl \
    -X POST \
    -H "Content-Type: application/json" \
    -H "X-Credential: ${CREDENTIAL}" \
    --data '{"query": "query IntrospectionQuery { __schema { queryType { name } mutationType { name } subscriptionType { name } types { ...FullType } directives { name description locations args { ...InputValue } } } } fragment FullType on __Type { kind name description fields(includeDeprecated: true) { name description args { ...InputValue } type { ...TypeRef } isDeprecated deprecationReason } inputFields { ...InputValue } interfaces { ...TypeRef } enumValues(includeDeprecated: true) { name description isDeprecated deprecationReason } possibleTypes { ...TypeRef } } fragment InputValue on __InputValue { name description type { ...TypeRef } defaultValue } fragment TypeRef on __Type { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name } } } } } } } }"}' \
    -o schema.json \
    https://realtime.${LIBCAFFEINE_DOMAIN}public/graphql/query

set +e

caffql --schema schema.json --output src/CaffQL.hpp --absl
rm schema.json
