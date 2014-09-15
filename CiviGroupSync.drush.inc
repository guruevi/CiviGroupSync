<?php
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
	
	return $items;
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
			drush_print ("Resulting Group: " . json_encode ( CiviGroupSync_getGroupContact ( $targetid ) ) );
		}
	}
}
function CiviGroupSync_getGroupContact($groupid) {
	$return = array ();
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
		continue;
	}
	
	if ($result ['count'] != 1) {
		drush_print ( "Invalid group specified" );
		return false;
	}
	if (in_array ( 1, $result ['values'] [$groupid] ["group_type"] )) {
		try {
			// entity=GroupContact&action=get&group_id=4
			$result = civicrm_api ( 'GroupContact', 'get', array (
					'group_id' => $groupid,
					'version' => 3,
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
			continue;
		}
		foreach ( $result ['values'] as $entity ) {
			// "id":"6","group_id":"4","contact_id":"4","status":"Added"
			if ($entity ["status"] == "Added") {
				$return [] = $entity ["contact_id"];
			}
		}
	} elseif (in_array ( 2, $result ['values'] [$groupid] ["group_type"] )) {
		// This is probably more expensive than doing it the other way:
		// This also doesn't work if the group id doesn't exist (it returns all objects)
		// entity=Contact&action=get&debug=1&sequential=1&json=1&group=2
		try {
			// entity=GroupContact&action=get&group_id=4
			$result = civicrm_api ( 'Contact', 'get', array (
					'group' => $groupid,
					'version' => 3,
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
			continue;
		}
		foreach ( $result ['values'] as $entity ) {
			$return [] = $entity ["id"];
		}
	} else {
		die ( dt ( "Invalid or unhandled group type: " ) . print_r ( $result ['values'] [$groupid] ["group_type"], 1 ) );
	}
	return $return;
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