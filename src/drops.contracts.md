<h1 class="contract">transfer</h1>

---
spec_version: "0.2.0"
title: transfer
summary: 'Transfer Drop(s)'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

{{from}} agrees to transfer {{drops_ids}} drops(s) to {{to}}.

{{#if memo}}There is a memo attached to the transfer stating:
{{memo}}
{{/if}}

There is a notification to be sent to {{to}}.

<h1 class="contract">destroy</h1>

---
spec_version: "0.2.0"
title: destroy
summary: 'Destroy Drop(s)'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

{{owner}} agrees to destroy {{drops_ids}} drops(s).

{{#if memo}}There is a memo attached to the transfer stating:
{{memo}}
{{/if}}

{{#if_has_value to_notify}}There is a notification to be sent to {{to_notify}}.
{{/if_has_value}}

<h1 class="contract">bind</h1>

---
spec_version: "0.2.0"
title: bind
summary: 'Bind Drop(s)'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

{{owner}} agrees to bind {{drops_ids}} drops(s).

<h1 class="contract">unbind</h1>

---
spec_version: "0.2.0"
title: unbind
summary: 'Unbind Drop(s)'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

{{owner}} agrees to unbind {{drops_ids}} drops(s).

<h1 class="contract">generate</h1>

---
spec_version: "0.2.0"
title: generate
summary: 'Generate Drop(s)'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

{{owner}} agrees to generate {{amount}} bound={{bound}} drops(s) using {{data}} data.

{{#if_has_value to_notify}}There is a notification to be sent to {{to_notify}}.
{{/if_has_value}}

<h1 class="contract">open</h1>

---
spec_version: "0.2.0"
title: open
summary: 'Open account balance'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

Opens RAM balance for {{owner}}.

<h1 class="contract">claim</h1>

---
spec_version: "0.2.0"
title: claim
summary: 'Claim remaining RAM balance'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

Claim any unclaimed RAM balance from the contract back to the {{owner}}'s account.

<h1 class="contract">enable</h1>

---
spec_version: "0.2.0"
title: enable
summary: 'Enable Drops contrat'
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

<h1 class="contract">test</h1>

---
spec_version: "0.2.0"
title: test
summary: test
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

<h1 class="contract">cleartable</h1>

---
spec_version: "0.2.0"
title: cleartable
summary: cleartable
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

<h1 class="contract">logdrops</h1>

---
spec_version: "0.2.0"
title: logdrops
summary: logdrops
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---

<h1 class="contract">logrambytes</h1>

---
spec_version: "0.2.0"
title: logrambytes
summary: logrambytes
icon: https://avatars.githubusercontent.com/u/158113782#d3bf290fddeddbb7d32aa897e9f7e9e13a2ae44956142e23eb47b77096a2ea8d
---
