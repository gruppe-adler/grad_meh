FROM alpine:3.10

COPY update-versionfile.sh /update-versionfile.sh

RUN apk update && apk upgrade && \
    apk add --no-cache bash git openssh

RUN apk add --update bash && rm -rf /var/cache/apk/*

ENTRYPOINT ["/update-versionfile.sh"]