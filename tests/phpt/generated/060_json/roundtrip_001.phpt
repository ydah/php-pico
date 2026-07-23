--TEST--
JSON encode and decode vector 001
--FILE--
<?php
$value = ['name' => 'item01', 'number' => 14, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item01","number":14,"enabled":false}:item01:14:0
