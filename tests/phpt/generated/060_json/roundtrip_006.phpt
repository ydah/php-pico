--TEST--
JSON encode and decode vector 006
--FILE--
<?php
$value = ['name' => 'item06', 'number' => 79, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item06","number":79,"enabled":true}:item06:79:1
