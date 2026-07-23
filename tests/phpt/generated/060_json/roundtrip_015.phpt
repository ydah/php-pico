--TEST--
JSON encode and decode vector 015
--FILE--
<?php
$value = ['name' => 'item15', 'number' => 196, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item15","number":196,"enabled":false}:item15:196:0
