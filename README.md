CiviGroupSync
=============

Drush plugin to sync CiviCRM (Smart) Groups to ACL groups

1) Put it in ~/.drush or anywhere else Drush looks for commands (/usr/drush/commands etc.)
2) Use drush cc drush to clear the local command cache.

- Use the --test and --print options to test and see the output
- Do "drush civigroup-sync fromID toID" or "drush civigroup-merge fromID toID" to sync 2 groups
- Schedule a cron job if you want to sync Smart groups to ACL groups

<h2>Usage</h2>
You can sync any number of groups to any number of groups. There is a civigroup-sync and civigroup-merge command (right now)

<h3>Sync</h3>
civigroup-sync can sync a number of source groups to a number of target groups. Using synchronize will delete users from a group if they are not in the source group(s)

<h3>Merge</h3>
civigroup-merge merges a number of source groups with their target groups. It does not delete any users, it just adds them.

<h3>Multi-group</h3>
Both sources and target options can take comma-separated values. For source groups, the entities in the groups are simply added together for action on the target group. When multiple target groups are specified, the requested action is completed for each target group.

<pre>
Source Group 1:
User 1, 2, 3
Source Group 2:
User 2, 3, 4
Action Sync to Group 5
Result:
User 1, 2, 3, 4 -> Group 5
</pre><pre>
Source Group 1:
User 1, 2
Source Group 2:
User 2, 4
Target Group 5:
User 2, 3, 6
Target Group 6:
User 2, 6, 7
Action Merge to Group 5, 6
Result:
Group 5: User 1, 2, 3, 4, 6
Group 6: User 1, 2, 4, 6, 7
</pre>

<h2>Warning</h2>
This script works for me but is largely untested. It should work as intended but there may be instances where it doesn't work properly. Use this on a dev system first and make a backup of your system before using it.
