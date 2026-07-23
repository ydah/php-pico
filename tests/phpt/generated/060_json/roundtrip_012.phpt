--TEST--
JSON encode and decode vector 012
--FILE--
<?php
$value = ['name' => 'item12', 'number' => 157, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item12","number":157,"enabled":true}:item12:157:1
