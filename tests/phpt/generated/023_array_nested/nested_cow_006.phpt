--TEST--
nested array copy-on-write vector 006
--FILE--
<?php
$tree = ['node' => ['value' => 25, 'items' => [26]]];
$copy = $tree;
$copy['node']['value'] += 8;
$copy['node']['items'][] = 35;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
25:33:26:26,35
