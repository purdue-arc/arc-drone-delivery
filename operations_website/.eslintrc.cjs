const isProduction = process.env.NODE_ENV === "production";

module.exports = {
	root: true,
	env: {
		node: true,
	},
	parser: "@typescript-eslint/parser",
	"plugins": [
		"solid",
		"unused-imports",
		"@stylistic/js",
		"jsdoc",  // https://www.npmjs.com/package/eslint-plugin-jsdoc
	],
	"extends": [
		"plugin:solid/typescript",
		"eslint:recommended",
		"plugin:jsdoc/recommended-typescript",
	],
	parserOptions: {
		ecmaVersion: 2020,
	},
	ignorePatterns: ["**/node_modules/*", "**/dist/*", "**/.solid/*", "**/gql/*"],
	rules: {
		"no-console": isProduction ? "warn" : "off",
		"no-debugger": isProduction ? "warn" : "off",
		quotes: ["error", "double"],  // Double-quote strings
		"semi-spacing": ["error", {"before": false, "after": true}],  // Semicolons immediately after code
		semi: ["error", "always"],  // Semicolons at the end of lines
		"comma-spacing": ["error", {"before": false, "after": true}],  // Spaces after commas in array
		"comma-dangle": ["error", "always-multiline"],  // comma after the last item in multiline array
		"space-before-function-paren": ["error", "never"],  // disallows space between yes and () in "function yes()"
		"brace-style": ["warn", "1tbs", { "allowSingleLine": true }], // Put opening brace on same line as keyword
		// TODO: this errors
		//"@stylistic/js/object-curly-spacing": ["error", "always"],
		"space-before-blocks": ["error", "always"],  // Enforce space between function () and {
		"arrow-spacing": ["error", {"before": true, "after": true}],  // Enforce space between arrow in arrow functions
		"keyword-spacing": ["error", { "before": true, "after": true }],  // Enforces spacing before & after keywords
		"switch-colon-spacing": ["error", {"before": false, "after": true}],  // Space after case: foo
		"key-spacing": "error",  // Space after {key: val}
		"no-unused-vars": "off",
		"unused-imports/no-unused-imports": "error",
		"unused-imports/no-unused-vars": [isProduction ? "error" : "warn", { "vars": "all", "varsIgnorePattern": "^_", "argsIgnorePattern": "^_" }],
		/*"one-line": [ true, "check-catch", "check-finally", "check-open-brace", "check-whitespace" ],*/

		"no-trailing-spaces": "error",  // Exactly what it sounds like

		"jsdoc/require-jsdoc": ["error", {"publicOnly": true}],
		"jsdoc/require-returns": "off",
		// These two rules can overlook some properties that should be documented, but also prevents erroneous warnings
		"jsdoc/require-param": ["warn", {"checkDestructuredRoots": false}],
		"jsdoc/check-param-names": ["warn", {"checkDestructured": false}],

		// "@typescript-eslint/no-explicit-any": ["warn"],
		// "@typescript-eslint/no-non-null-assertion": ["off"],
		// "@typescript-eslint/explicit-module-boundary-types": ["error"],
		// "@typescript-eslint/ban-types": ["error", { "types": { "String": false, "Boolean": false, "Number": false, "Symbol": false, "{}": false, "Object": false, "object": false, "Function": false, }, "extendDefaults": true } ],
	},
};
