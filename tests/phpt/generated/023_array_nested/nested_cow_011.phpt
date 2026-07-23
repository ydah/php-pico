--TEST--
nested array copy-on-write vector 011
--FILE--
<?php
$tree = ['node' => ['value' => 45, 'items' => [46]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 53;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
45:51:46:46,53
