#!/bin/sh
# Introspects and generates client code for the realtime graphql endpoint.
# Assumes caffql, curl, and jq are in your path.

set -e

CREDENTIAL=$(curl "https://api.staging.caffeine.tv/v1/credentials/anonymous" | jq -j .credential)

curl \
    -X POST \
    -H "Content-Type: application/json" \
    -H "X-Credential: ${CREDENTIAL}" \
    --data '{"query": "query IntrospectionQuery { __schema { queryType { name } mutationType { name } subscriptionType { name } types { ...FullType } directives { name description locations args { ...InputValue } } } } fragment FullType on __Type { kind name description fields(includeDeprecated: true) { name description args { ...InputValue } type { ...TypeRef } isDeprecated deprecationReason } inputFields { ...InputValue } interfaces { ...TypeRef } enumValues(includeDeprecated: true) { name description isDeprecated deprecationReason } possibleTypes { ...TypeRef } } fragment InputValue on __InputValue { name description type { ...TypeRef } defaultValue } fragment TypeRef on __Type { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name ofType { kind name } } } } } } } }"}' \
    -o schema.json \
    https://realtime.staging.caffeine.tv/public/graphql/query

set +e

caffql --schema schema.json --output src/CaffQL.hpp --absl
rm schema.json
