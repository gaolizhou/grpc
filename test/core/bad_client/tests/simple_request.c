/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "test/core/bad_client/bad_client.h"

#include <string.h>

#include "test/core/end2end/cq_verifier.h"
#include "src/core/surface/server.h"

#define PFX_STR                                                            \
  "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"                                       \
  "\x00\x00\x00\x04\x00\x00\x00\x00\x00" /* settings frame */              \
  "\x00\x00\xc9\x01\x04\x00\x00\x00\x01" /* headers: generated from        \
                                            simple_request.headers in this \
                                            directory */                   \
  "\x10\x05:path\x08/foo/bar"                                              \
  "\x10\x07:scheme\x04http"                                                \
  "\x10\x07:method\x04POST"                                                \
  "\x10\x0a:authority\x09localhost"                                        \
  "\x10\x0c"                                                               \
  "content-type\x10"                                                       \
  "application/grpc"                                                       \
  "\x10\x14grpc-accept-encoding\x15identity,deflate,gzip"                  \
  "\x10\x02te\x08trailers"                                                 \
  "\x10\x0auser-agent\"bad-client grpc-c/0.12.0.0 (linux)"

static void *tag(gpr_intptr t) { return (void *)t; }

static void verifier(grpc_server *server, grpc_completion_queue *cq) {
  grpc_call_error error;
  grpc_call *s;
  grpc_call_details call_details;
  cq_verifier *cqv = cq_verifier_create(cq);
  grpc_metadata_array request_metadata_recv;

  grpc_call_details_init(&call_details);
  grpc_metadata_array_init(&request_metadata_recv);

  error = grpc_server_request_call(server, &s, &call_details,
                                   &request_metadata_recv, cq, cq, tag(101));
  GPR_ASSERT(GRPC_CALL_OK == error);
  cq_expect_completion(cqv, tag(101), 1);
  cq_verify(cqv);

  GPR_ASSERT(0 == strcmp(call_details.host, "localhost"));
  GPR_ASSERT(0 == strcmp(call_details.method, "/foo/bar"));

  grpc_metadata_array_destroy(&request_metadata_recv);
  grpc_call_details_destroy(&call_details);
  grpc_call_destroy(s);
  cq_verifier_destroy(cqv);
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);

  /* basic request: check that things are working */
  GRPC_RUN_BAD_CLIENT_TEST(verifier, PFX_STR, 0);

  /* push an illegal data frame */
  GRPC_RUN_BAD_CLIENT_TEST(verifier, PFX_STR
                           "\x00\x00\x05\x00\x00\x00\x00\x00\x01"
                           "\x34\x00\x00\x00\x00",
                           0);

  /* push a data frame with bad flags */
  GRPC_RUN_BAD_CLIENT_TEST(verifier,
                           PFX_STR "\x00\x00\x00\x00\x02\x00\x00\x00\x01", 0);

  return 0;
}