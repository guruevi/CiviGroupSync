<?php
/**
 * CiviGroupSync Drush Command File
 * Copyright (C) 2014 Evi Vanoost (evi.vanoost@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
function CiviGroupSync_drush_command() {
	$items = array ();
	
	// Default options for all CGS commands
	$defopts = array (
			'print' => "Output the resulting group to terminal",
			'test' => "Does not commit the changes" 
	);
	// Default arguments for all CGS commands
	$defargs = array (
			'sourceid' => 'Source Group ID or ID\'s separated with a comma',
			'targetid' => 'Target Group ID or ID\'s separated with a comma' 
	);
	$defitem = array (
			'arguments' => $defargs,
			'required-arguments' => TRUE,
			'options' => $defopts 
	);
	
	// the key in the $items array is the name of the command.
	$items ['civigroup-sync'] = array_merge ( array (
			'description' => "Sync group(s), deletes entities in the target that are not in the source(s)" 
	), $defitem );
	
	$items ['civigroup-merge'] = array (
			'description' => "Merge group(s), union of source(s) and target(s))" 
	) + $defitem;
	
	$items ['civigroup-empty'] = array (
			'description' => "Empties the group(s), deletes entities from a group)",
			'arguments' => array (
					'targetid' => 'Target Group ID or ID\'s separated with a comma' 
			) 
	) + $defitem;
	
	$items ['civigroup-user-create'] = array (
			'description' => "Create Drupal users from a CiviCRM group",
			'arguments' => array (
					'sourceid' => 'Source Group ID or ID\'s separated with a comma' 
			) 
	) + $defitem;
	
	return $items;
}
function CiviGroupSync_user_search($email, $name) {
	// We use a DB query while looking for the uid to keep things speedy.
	$uids = array ();
	if (drush_drupal_major_version () >= 7) {
		$uid_query = db_query ( "SELECT uid, name FROM {users} WHERE mail = :mail OR name = :name", array (
				':mail' => $email,
				':name' => $name 
		) );
	} else {
		$uid_query = db_query ( "SELECT uid, name FROM {users} WHERE mail = '%s' OR name = '%s'", $email, $name );
	}
	while ( $uid = drush_db_fetch_object ( $uid_query ) ) {
		$uids [$uid->uid] = $uid->name;
	}
	return (count ( $uids ));
}
function drush_CiviGroupSync_civigroup_user_create($groupids) {
	$groups = CiviGroupSync_split ( $groupids );
	if (empty ( $groups )) {
		drush_set_error ( "Error validating groups for user create" );
		return false;
	}
	civicrm_initialize ();
	$final_group = array ();
	foreach ( $groups as $groupid ) {
		// Get all the entities from these groups
		$final_group += CiviGroupSync_getGroupContactDetails ( $groupid );
	}
	foreach ( $final_group as $entity ) {
		if ($error = user_validate_mail ( $entity ['email'] )) {
			drush_print ( $entity ['email'] . ": " . $error );
			continue;
		} elseif ($error = user_validate_name ( $entity ['nick_name'] )) {
			drush_print ( $entity ['nick_name'] . ": " . $error );
			continue;
		} elseif (CiviGroupSync_user_search ( $entity ['email'], $entity ['nick_name'] ) != 0) {
			if ($print = drush_get_option ( 'print' )) {
				drush_print ( "A user account for " . $entity ['nick_name'] . " already exists" );
			}
			continue;
		}
		
		$new_user = array (
				'name' => $entity ['nick_name'],
				'mail' => $entity ['email'],
				'pass' => user_password ( 10 ),
				'status' => 1,
				'init' => $entity ['email'] 
		);
		
		if (! drush_get_option ( 'test' )) {
			if ($account = user_save ( null, $new_user )) {
				if ($print = drush_get_option ( 'print' )) {
					drush_print ( "Account created: " . print_r ( $account, 1 ) );
				}
				_user_mail_notify ( 'register_admin_created', $account );
			} else {
				drush_print ( "Could not create a new user account with the name " . $entity ['nick_name'] . "." );
			}
		} else {
			drush_print ( "Account created (test): " . $entity ['nick_name'] );
		}
	}
}
function drush_CiviGroupSync_civigroup_sync($sourceids, $targetids) {
	$sources = CiviGroupSync_split ( $sourceids );
	$targets = CiviGroupSync_split ( $targetids );
	if (empty ( $sources ) || empty ( $targets )) {
		drush_set_error ( "Error validating groups for merge" );
		return false;
	}
	civicrm_initialize ();
	$final_group = array ();
	// For each of the sources
	foreach ( $sources as $sourceid ) {
		// Get all the entities from this source group
		$final_group = array_merge ( CiviGroupSync_getGroupContact ( $sourceid ), $final_group );
	}
	// Make sure we only get unique values
	$final_group = array_unique ( $final_group );
	
	foreach ( $targets as $targetid ) {
		// Get the current target group
		$this_group = CiviGroupSync_getGroupContact ( $targetid );
		
		// Find out if there are any entities in our target that are to be deleted
		$tobedeleted = array_diff ( $this_group, $final_group );
		
		// Delete entities to be deleted
		foreach ( $tobedeleted as $target_entity ) {
			CiviGroupSync_deleteGroupContact ( $targetid, $target_entity );
		}
		
		// Remove any source entities that are already in the group
		$this_final_group = array_diff ( $final_group, $this_group );
		foreach ( $this_final_group as $target_entity ) {
			// Create a contact in the group for each contact ID we have left
			CiviGroupSync_createGroupContact ( $targetid, $target_entity );
		}
		// If we need to print out the result, fetch the group and print it
		if ($print = drush_get_option ( 'print' )) {
			drush_print ( "Resulting Group: " . json_encode ( CiviGroupSync_getGroupContact ( $targetid ) ) );
		}
	}
}
function CiviGroupSync_getGroupContact($groupid) {
	$return = array ();
	if ($result = CiviGroupSync_getGroupContactDetails ( $groupid )) {
		foreach ( $result as $entity ) {
			$return [] = $entity ["id"];
		}
	}
	return $return;
}
function CiviGroupSync_getGroupContactDetails($groupid) {
	// First find out the type of group
	try {
		// entity=GroupContact&action=get&group_id=4
		$result = civicrm_api ( 'Group', 'get', array (
				'id' => $groupid,
				'version' => 3 
		) );
	} catch ( CiviCRM_API3_Exception $e ) {
		$errorMessage = $e->getMessage ();
		$errorCode = $e->getErrorCode ();
		$errorData = $e->getExtraParams ();
		drush_log ( dt ( "!error", array (
				'!error' => $errorData 
		), 'error' ) );
        return;
	}
	
	if ($result ['count'] != 1) {
		drush_print ( "Invalid group specified" );
		return false;
	}
	// This is probably more expensive than doing it the other way:
	// entity=Contact&action=get&debug=1&sequential=1&json=1&group=2
	try {
		// entity=GroupContact&action=get&group_id=4
		$result = civicrm_api ( 'Contact', 'get', array (
				'group' => $groupid,
				'version' => 3,
				'return' => 'nick_name,email',
				'options' => array (
						'limit' => 0 
				) 
		) );
	} catch ( CiviCRM_API3_Exception $e ) {
		$errorMessage = $e->getMessage ();
		$errorCode = $e->getErrorCode ();
		$errorData = $e->getExtraParams ();
		drush_log ( dt ( "!error", array (
				'!error' => $errorData 
		), 'error' ) );
        return;
	}
	return $result ['values'];
}
function CiviGroupSync_createGroupContact($groupid, $contactid) {
	if (drush_get_option ( 'print' )) {
		drush_print ( "Adding User ID $contactid to Group ID $groupid" );
	}
	if (drush_get_option ( 'test' )) {
		return true;
	}
	try {
		// entity=GroupContact&action=create&group_id=4&contact_id=
		$result = civicrm_api ( 'GroupContact', 'create', array (
				'contact_id' => $contactid,
				'group_id' => $groupid,
				'version' => 3 
		) );
	} catch ( CiviCRM_API3_Exception $e ) {
		$errorMessage = $e->getMessage ();
		$errorCode = $e->getErrorCode ();
		$errorData = $e->getExtraParams ();
		drush_log ( dt ( "!error", array (
				'!error' => $errorData 
		), 'error' ) );
		return false;
	}
	return true;
}
function CiviGroupSync_deleteGroupContact($groupid, $contactid) {
	if (drush_get_option ( 'test' )) {
		drush_print ( "Removing User ID $contactid from Group ID $groupid" );
	}
	if (drush_get_option ( 'test' )) {
		return true;
	}
	try {
		// entity=GroupContact&action=create&group_id=4&contact_id=
		$result = civicrm_api ( 'GroupContact', 'delete', array (
				'contact_id' => $contactid,
				'group_id' => $groupid,
				'version' => 3 
		) );
	} catch ( CiviCRM_API3_Exception $e ) {
		$errorMessage = $e->getMessage ();
		$errorCode = $e->getErrorCode ();
		$errorData = $e->getExtraParams ();
		drush_log ( dt ( "!error", array (
				'!error' => $errorData 
		), 'error' ) );
		return false;
	}
	return true;
}
function drush_CiviGroupSync_civigroup_merge($sourceids, $targetids) {
	$sources = CiviGroupSync_split ( $sourceids );
	$targets = CiviGroupSync_split ( $targetids );
	if (empty ( $sources ) || empty ( $targets )) {
		drush_set_error ( "Error validating groups for merge" );
		return false;
	}
	civicrm_initialize ();
	$final_group = array ();
	// For each of the sources
	foreach ( $sources as $sourceid ) {
		// Get all the entities from this source group
		$final_group = array_merge ( CiviGroupSync_getGroupContact ( $sourceid ), $final_group );
	}
	
	// Make sure we only get unique values
	$final_group = array_unique ( $final_group );
	
	// For each of the targets, "merge" them with the final group and save them
	foreach ( $targets as $targetid ) {
		// First we need to fetch the entities already in this group (so we don't add an already existing entity)
		
		$this_group = CiviGroupSync_getGroupContact ( $targetid );
		
		// Filter out the already existing entities
		$this_final_group = array_diff ( $final_group, $this_group );
		
		// Create a contact in the group for each contact ID we have left
		foreach ( $this_final_group as $contactid ) {
			CiviGroupSync_createGroupContact ( $targetid, $contactid );
		}
		
		// If we need to print out the result, fetch the group and print it
		if ($print = drush_get_option ( 'print' )) {
			drush_print ( "Resulting Group:" . json_encode ( CiviGroupSync_getGroupContact ( $targetid ) ) );
		}
	}
}
function drush_CiviGroupSync_civigroup_empty($targetid) {
	// TODO: Implement deleting the contents of a group
}
function CiviGroupSync_split($groups) {
	$groups = explode ( ",", $groups );
	
	foreach ( $groups as $groupid ) {
		if (! is_numeric ( $groupid )) {
			drush_set_error ( "Invalid Group ID specified" );
			return false;
		}
	}
	return $groups;
}

?>
