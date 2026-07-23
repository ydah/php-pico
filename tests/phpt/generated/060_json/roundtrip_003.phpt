--TEST--
JSON encode and decode vector 003
--FILE--
<?php
$value = ['name' => 'item03', 'number' => 40, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item03","number":40,"enabled":false}:item03:40:0
