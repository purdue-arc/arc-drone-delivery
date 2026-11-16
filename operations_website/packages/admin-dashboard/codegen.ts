import type {CodegenConfig} from '@graphql-codegen/cli'
import {nhost} from "./src/lib/nHost";
import {loadEnv} from "vite";

// Documentation: https://the-guild.dev/graphql/codegen/docs/config-reference/codegen-config

const config: CodegenConfig = {
  schema: [{
    [nhost.graphql.httpUrl]: {
      headers: {
        "x-hasura-admin-secret": loadEnv("production", process.cwd())["VITE_HASURA_ADMIN_SECRET"] as string,
      },
    },
  }],
  documents: ['src/**/*.tsx', 'src/**/*.ts'],
  ignoreNoDocuments: true, // for better experience with the watcher
  generates: {
    './src/gql/': {
      preset: 'client',
      config: {
        useTypeImports: true
      }
    },
    // Docs: https://the-guild.dev/graphql/codegen/plugins/other/schema-ast
    './src/gql/schema.graphql': {
      plugins: ['schema-ast']
    }
  }
}

export default config
