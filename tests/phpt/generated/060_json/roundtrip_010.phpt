--TEST--
JSON encode and decode vector 010
--FILE--
<?php
$value = ['name' => 'item10', 'number' => 131, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item10","number":131,"enabled":true}:item10:131:1
