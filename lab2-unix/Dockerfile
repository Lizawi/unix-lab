# docker volume create shared && docker build -t concurrent .
# docker run -d -v shared:/data concurrent
FROM alpine:3.20
RUN apk add --no-cache flock
COPY concurrent.sh /
VOLUME /data
CMD ["sh", "/concurrent.sh"]
