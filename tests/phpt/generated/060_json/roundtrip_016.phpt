--TEST--
JSON encode and decode vector 016
--FILE--
<?php
$value = ['name' => 'item16', 'number' => 209, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item16","number":209,"enabled":true}:item16:209:1
