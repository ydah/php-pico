--TEST--
JSON encode and decode vector 004
--FILE--
<?php
$value = ['name' => 'item04', 'number' => 53, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item04","number":53,"enabled":true}:item04:53:1
