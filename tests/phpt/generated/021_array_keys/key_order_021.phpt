--TEST--
associative key order vector 021
--FILE--
<?php
$items = ['left' => 31, 4 => 83, 'right' => 135];
unset($items[4]);
$items['tail'] = 136;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:31,135,136:1
