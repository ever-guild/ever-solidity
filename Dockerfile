# TODO use FROM alpine:20221110
FROM ubuntu:20.04
COPY /sold /usr/bin/sold
ENTRYPOINT ["sold"]
WORKDIR /src
CMD ["--help"]
