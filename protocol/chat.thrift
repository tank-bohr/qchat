namespace cpp qchat.rpc

// Public, anonymous room. Display names are labels, never identities.
struct Message {
  1: required i64 id,
  2: required i64 timestampMs,
  3: required string displayName,
  4: required string text
}

struct MessagePage {
  1: required string serverId,
  2: required list<Message> messages,
  3: required i64 nextCursor,
  4: required bool reset
}

exception InvalidRequest {
  1: required string reason
}

service Chat {
  Message post(1: string displayName, 2: string text)
    throws (1: InvalidRequest invalid),
  // An empty/stale serverId or an expired cursor requests a fresh history.
  MessagePage poll(1: string serverId, 2: i64 afterId, 3: i32 limit)
    throws (1: InvalidRequest invalid)
}
