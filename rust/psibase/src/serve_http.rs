use crate::{
    create_schema, generate_action_templates, reflect::Reflect, HttpReply, HttpRequest,
    ProcessActionStruct, WithActionStruct,
};
use async_graphql::{
    http::{graphiql_source, receive_body},
    EmptyMutation, EmptySubscription,
};
use futures::executor::block_on;
use serde_json::to_vec;

const SIMPLE_UI: &[u8] = br#"<html><div id="root" class="ui container"></div><script src="/common/SimpleUI.mjs" type="module"></script></html>"#;

/// Serve a developer UI
///
/// This function serves a simple developer UI to help get you started. The UI it
/// generates is not suitable for end users.
///
/// This serves the following:
/// - `GET /` or `GET /index.html`: provided by [serve_simple_index]
/// - `GET /action_templates`: provided by [serve_action_templates]
/// - `GET /schema`: provided by [serve_schema]
/// - `POST /pack_action/x`: provided by [serve_pack_action]
///
/// `Wrapper` should be generated by [`#[psibase::service]`](macro@crate::service).
pub fn serve_simple_ui<Wrapper: Reflect + WithActionStruct>(
    request: &HttpRequest,
) -> Option<HttpReply> {
    None.or_else(|| serve_simple_index(request))
        .or_else(|| serve_schema::<Wrapper>(request))
        .or_else(|| serve_action_templates::<Wrapper>(request))
        .or_else(|| serve_pack_action::<Wrapper>(request))
}

/// Serve index.html for a developer UI
///
/// This function serves the following HTML for `GET /` or `GET /index.html`:
///
/// ```html
/// <html>
///     <div id="root" class="ui container"></div>
///     <script src="/common/SimpleUI.mjs" type="module"></script>
/// </html>
/// ```
pub fn serve_simple_index(request: &HttpRequest) -> Option<HttpReply> {
    if request.method == "GET" && (request.target == "/" || request.target == "/index.html") {
        Some(HttpReply {
            contentType: "text/html".into(),
            body: SIMPLE_UI.to_vec(),
            headers: vec![],
        })
    } else {
        None
    }
}

/// Handle `/schema` request
///
/// If `request` is a `GET /schema`, then this returns a
/// [schema](https://doc-sys.psibase.io/format/schema.html) which
/// defines the service interface.
///
/// If `request` doesn't match the above, then this returns `None`.
///
/// `Wrapper` should be generated by [`#[psibase::service]`](macro@crate::service).
pub fn serve_schema<Wrapper: Reflect>(request: &HttpRequest) -> Option<HttpReply> {
    if request.method == "GET" && request.target == "/schema" {
        Some(HttpReply {
            contentType: "text/html".into(),
            body: to_vec(&create_schema::<Wrapper>()).unwrap(),
            headers: vec![],
        })
    } else {
        None
    }
}

/// Handle `/action_templates` request
///
/// If `request` is a `GET /action_templates`, then this returns a
/// JSON object containing a field for each action in `Service`. The
/// field names match the action names. The field values are objects
/// with the action arguments, each containing sample data.
///
/// If `request` doesn't match the above, then this returns `None`.
///
/// `Wrapper` should be generated by [`#[psibase::service]`](macro@crate::service).
pub fn serve_action_templates<Wrapper: Reflect>(request: &HttpRequest) -> Option<HttpReply> {
    if request.method == "GET" && request.target == "/action_templates" {
        Some(HttpReply {
            contentType: "text/html".into(),
            body: generate_action_templates::<Wrapper>().into(),
            headers: vec![],
        })
    } else {
        None
    }
}

/// Handle `/pack_action/` request
///
/// If `request` is a `POST /pack_action/x`, where `x` is an action
/// on `Wrapper`, then this parses a JSON object containing the arguments
/// to `x`, packs them using fracpack, and returns the result as an
/// `application/octet-stream`.
///
/// If `request` doesn't match the above, or the action name is not found,
/// then this returns `None`.
///
/// `Wrapper` should be generated by [`#[psibase::service]`](macro@crate::service).
pub fn serve_pack_action<Wrapper: WithActionStruct>(request: &HttpRequest) -> Option<HttpReply> {
    struct PackAction<'a>(&'a [u8]);

    impl<'a> ProcessActionStruct for PackAction<'a> {
        type Output = HttpReply;

        fn process<
            Return: Reflect + fracpack::PackableOwned + serde::Serialize + serde::de::DeserializeOwned,
            ArgStruct: Reflect + fracpack::PackableOwned + serde::Serialize + serde::de::DeserializeOwned,
        >(
            self,
        ) -> Self::Output {
            HttpReply {
                contentType: "application/octet-stream".into(),
                body: serde_json::from_slice::<ArgStruct>(self.0)
                    .unwrap()
                    .packed(),
                headers: vec![],
            }
        }
    }

    if request.method == "POST" && request.target.starts_with("/pack_action/") {
        Wrapper::with_action_struct(&request.target[13..], PackAction(&request.body))
    } else {
        None
    }
}

/// Handle `/graphql` request
///
/// This handles graphql requests, including fetching the schema.
///
/// * `GET /graphql`: Returns the schema.
/// * `GET /graphql?query=...`: Run query in URL and return JSON result.
/// * `POST /graphql?query=...`: Run query in URL and return JSON result.
/// * `POST /graphql` with `Content-Type = application/graphql`: Run query that's in body and return JSON result.
/// * `POST /graphql` with `Content-Type = application/json`: Body contains a JSON object of the form `{"query"="..."}`. Run query and return JSON result.
pub fn serve_graphql<Query: async_graphql::ObjectType + 'static>(
    request: &HttpRequest,
    query: Query,
) -> Option<HttpReply> {
    let (base, args) = if let Some((b, q)) = request.target.split_once('?') {
        (b, q)
    } else {
        (request.target.as_ref(), "")
    };
    if base != "/graphql" || request.method != "GET" && request.method != "POST" {
        return None;
    }
    block_on(async move {
        let schema = async_graphql::Schema::new(query, EmptyMutation, EmptySubscription);
        if let Some(request) = args.strip_prefix("?query=") {
            let res = schema.execute(request).await;
            Some(HttpReply {
                contentType: "application/json".into(),
                body: serde_json::to_vec(&res).unwrap(),
                headers: vec![],
            })
        } else if request.method == "GET" {
            Some(HttpReply {
                contentType: "text".into(), // TODO
                body: schema.sdl().into(),
                headers: vec![],
            })
        } else {
            let request = receive_body(
                Some(&request.contentType),
                request.body.as_ref(),
                Default::default(),
            )
            .await
            .unwrap();
            let res = schema.execute(request).await;
            Some(HttpReply {
                contentType: "application/json".into(),
                body: serde_json::to_vec(&res).unwrap(),
                headers: vec![],
            })
        }
    })
}

/// Serve GraphiQL UI
///
/// This function serves the GraphiQL UI for `GET /graphiql.html`.
/// Use with [serve_graphql].
///
/// This wraps [graphiql_source].
pub fn serve_graphiql(request: &HttpRequest) -> Option<HttpReply> {
    if request.method == "GET" && request.target == "/graphiql.html" {
        Some(HttpReply {
            contentType: "text/html".into(),
            body: graphiql_source("/graphql", None).into(),
            headers: vec![],
        })
    } else {
        None
    }
}
