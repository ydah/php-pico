--TEST--
JSON encode and decode vector 000
--FILE--
<?php
$value = ['name' => 'item00', 'number' => 1, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item00","number":1,"enabled":true}:item00:1:1
