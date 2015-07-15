// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: health.proto
#region Designer generated code

using System;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;

namespace Grpc.Health.V1Alpha {
  public static class Health
  {
    static readonly string __ServiceName = "grpc.health.v1alpha.Health";

    static readonly Marshaller<global::Grpc.Health.V1Alpha.HealthCheckRequest> __Marshaller_HealthCheckRequest = Marshallers.Create((arg) => arg.ToByteArray(), global::Grpc.Health.V1Alpha.HealthCheckRequest.ParseFrom);
    static readonly Marshaller<global::Grpc.Health.V1Alpha.HealthCheckResponse> __Marshaller_HealthCheckResponse = Marshallers.Create((arg) => arg.ToByteArray(), global::Grpc.Health.V1Alpha.HealthCheckResponse.ParseFrom);

    static readonly Method<global::Grpc.Health.V1Alpha.HealthCheckRequest, global::Grpc.Health.V1Alpha.HealthCheckResponse> __Method_Check = new Method<global::Grpc.Health.V1Alpha.HealthCheckRequest, global::Grpc.Health.V1Alpha.HealthCheckResponse>(
        MethodType.Unary,
        "Check",
        __Marshaller_HealthCheckRequest,
        __Marshaller_HealthCheckResponse);

    // client-side stub interface
    public interface IHealthClient
    {
      global::Grpc.Health.V1Alpha.HealthCheckResponse Check(global::Grpc.Health.V1Alpha.HealthCheckRequest request, CancellationToken token = default(CancellationToken));
      Task<global::Grpc.Health.V1Alpha.HealthCheckResponse> CheckAsync(global::Grpc.Health.V1Alpha.HealthCheckRequest request, CancellationToken token = default(CancellationToken));
    }

    // server-side interface
    public interface IHealth
    {
      Task<global::Grpc.Health.V1Alpha.HealthCheckResponse> Check(ServerCallContext context, global::Grpc.Health.V1Alpha.HealthCheckRequest request);
    }

    // client stub
    public class HealthClient : AbstractStub<HealthClient, StubConfiguration>, IHealthClient
    {
      public HealthClient(Channel channel) : this(channel, StubConfiguration.Default)
      {
      }
      public HealthClient(Channel channel, StubConfiguration config) : base(channel, config)
      {
      }
      public global::Grpc.Health.V1Alpha.HealthCheckResponse Check(global::Grpc.Health.V1Alpha.HealthCheckRequest request, CancellationToken token = default(CancellationToken))
      {
        var call = CreateCall(__ServiceName, __Method_Check);
        return Calls.BlockingUnaryCall(call, request, token);
      }
      public Task<global::Grpc.Health.V1Alpha.HealthCheckResponse> CheckAsync(global::Grpc.Health.V1Alpha.HealthCheckRequest request, CancellationToken token = default(CancellationToken))
      {
        var call = CreateCall(__ServiceName, __Method_Check);
        return Calls.AsyncUnaryCall(call, request, token);
      }
    }

    // creates service definition that can be registered with a server
    public static ServerServiceDefinition BindService(IHealth serviceImpl)
    {
      return ServerServiceDefinition.CreateBuilder(__ServiceName)
          .AddMethod(__Method_Check, serviceImpl.Check).Build();
    }

    // creates a new client stub
    public static IHealthClient NewStub(Channel channel)
    {
      return new HealthClient(channel);
    }

    // creates a new client stub
    public static IHealthClient NewStub(Channel channel, StubConfiguration config)
    {
      return new HealthClient(channel, config);
    }
  }
}
#endregion