#!/bin/bash

# See https://github.com/CircleCI-Public/slack-orb/issues/208#issuecomment-859789397

set -eufo pipefail

# Issues a v1.1 CircleCI API request to the given relative path within this
# project, and outputs the JSON response to stdout. Specify the HTTP request
# method as the second argument. CIRCLE_TOKEN is not a CircleCI built-in and
# must be provided by a context. It must be a CircleCI API token with access to
# this project and the required scope. The environment variable VCS_TYPE is not
# a CircleCI built-in and should be set from the CircleCI config.yml file to
# << pipeline.project.type >>. See
# https://circleci.com/docs/2.0/pipeline-variables/#pipeline-values.
makeApiRequest() {
  curl \
    --request "$2" \
    --header "Content-Type: application/json" \
    --header "Accept: application/json" \
    --header "Circle-Token: $CIRCLE_TOKEN" \
    "https://circleci.com/api/v1.1/project/$VCS_TYPE/$CIRCLE_PROJECT_USERNAME/$CIRCLE_PROJECT_REPONAME/$1"
}

# Output the value of the given JSON key to stdout. The second parameter is the
# JSON string to search.
getJsonValue() {
  local match="$(grep --only-matching "\"$1\"\s*:\s*\"[^\"]*\"" <<< "$2")"
  # Get the value between the third and fourth double quotes.
  cut --delimiter=\" --fields=4 <<< "$match"
}

# Trick the Slack orb into reporting the status of the most recently completed
# job preceding this one that has the same VCS commit hash. Fail this job if the
# preceding one failed. Cancel this job if the preceding one was cancelled.
# Overwrite the CIRCLE_JOB environment variable with the prior job's name and
# the CIRCLE_BUILD_URL environment variable with the prior job's URL. The other
# CIRCLE_* environment variables that Slack reads from should be the same
# between this notify job and the corresponding prior job. If no corresponding
# job can be found, fail this job without tricking Slack.
trickSlackOrb() {
  local buildNum
  for ((buildNum = $CIRCLE_BUILD_NUM - 1; buildNum > 0; buildNum--))
  do
    # This API request requires that CIRCLE_TOKEN have at least the read only
    # scope. See https://circleci.com/docs/api/v1/#single-job.
    local jobMetadata="$(makeApiRequest "$buildNum" "GET")"

    local job="$(getJsonValue "CIRCLE_JOB" "$jobMetadata")"
    if [[ "$job" != "notify" ]] &&
      grep --quiet "\"vcs_revision\":\"$CIRCLE_SHA1\"" <<< "$jobMetadata" &&
      grep --quiet '"lifecycle":"finished"' <<< "$jobMetadata" &&
      grep --quiet '"dont_build":null' <<< "$jobMetadata"
    then
      if grep --quiet '"outcome":"canceled"' <<< "$jobMetadata"
      then
        # Cancels the currently executing CircleCI job, and outputs a JSON document
        # containing metadata regarding this job to stdout. See
        # https://circleci.com/docs/api/v1/#cancel-a-build. This API request
        # requires that CIRCLE_TOKEN token have the admin scope.
        makeApiRequest "$CIRCLE_BUILD_NUM/cancel" "POST"
        # Give the cancellation up to a minute to take effect before failing.
        sleep 60
        break
      fi
      local buildUrl="$(getJsonValue "build_url" "$jobMetadata")"
      echo "export CIRCLE_JOB=$job" >> $BASH_ENV
      echo "export CIRCLE_BUILD_URL=$buildUrl" >> $BASH_ENV
      grep --quiet '"outcome":"success"' <<< "$jobMetadata" && return
      break
    fi
  done
  exit 1
}

trickSlackOrb
