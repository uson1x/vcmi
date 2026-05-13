# Mine

Multiple mines per resource are allowed.

Beside the common parameters from [Map Object Format](../Map_Object_Format.md) there are some additional parameters:

```json
{
	/// produced resource
	"resource" : "mithril",

	/// amount of resources produced each day
	"defaultQuantity" : 1,

	/// displayed name of mine
	"name" : "name text",

	/// displayed description of mine (for popup)
	"description" : "description text",

	/// Optional guards, uses the same stack format as other configurable guard lists.
	"guards" : [
		{ "type" : "core:marksman", "amount" : 40, "upgradeChance" : 50 }
	],

	/// Optional message shown when hero attempts to visit guarded non-abandoned mine.
	/// Falls back to default abandoned messages when not set.
	"onGuardedMessage" : "This mine is guarded.\n\nDo you wish to fight the guards?",

	/// Optional image showed on kingdom overview (animation; only frame 0 displayed)
	"kingdomOverviewImage" : "image.def"
}
```
