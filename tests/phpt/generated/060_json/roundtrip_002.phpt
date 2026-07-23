--TEST--
JSON encode and decode vector 002
--FILE--
<?php
$value = ['name' => 'item02', 'number' => 27, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item02","number":27,"enabled":true}:item02:27:1
