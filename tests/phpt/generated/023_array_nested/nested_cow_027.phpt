--TEST--
nested array copy-on-write vector 027
--FILE--
<?php
$tree = ['node' => ['value' => 109, 'items' => [110]]];
$copy = $tree;
$copy['node']['value'] += 8;
$copy['node']['items'][] = 119;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
109:117:110:110,119
